// SPDX-License-Identifier: GPL-3.0-or-later
#include "kcm.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <algorithm>

using namespace Qt::StringLiterals;

K_PLUGIN_CLASS_WITH_JSON(SwyperKcm, "kcm_swyper.json")

AxisConfig::AxisConfig(tpg::AxisSetting *data, bool vertical, SwyperKcm *kcm)
    : QObject(kcm)
    , m_data(data)
    , m_vertical(vertical)
    , m_kcm(kcm)
{
}

void AxisConfig::set(QString &field, const QString &value)
{
    if (field == value) {
        return;
    }
    field = value;
    Q_EMIT changed();
    m_kcm->edited();
}

SwyperKcm::SwyperKcm(QObject *parent, const KPluginMetaData &data)
    : KQuickConfigModule(parent, data)
    , m_config(KSharedConfig::openConfig(u"swyperrc"_s, KConfig::NoGlobals))
{
    setButtons(Apply | Default);
    m_axes[0] = new AxisConfig(&m_current.three.vertical, true, this);
    m_axes[1] = new AxisConfig(&m_current.three.horizontal, false, this);
    m_axes[2] = new AxisConfig(&m_current.four.vertical, true, this);
    m_axes[3] = new AxisConfig(&m_current.four.horizontal, false, this);
}

SwyperKcm::~SwyperKcm()
{
    stopTest();
}

namespace
{
const QString kwinService = u"org.kde.KWin"_s;
const QString testPath = u"/Swyper"_s;
const QString testInterface = u"io.github.elisantana1.Swyper"_s;
}

void SwyperKcm::connectTestSignals(bool on)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (on) {
        bus.connect(kwinService, testPath, testInterface, u"Fingers"_s, this, SLOT(onTestFingers(int)));
        bus.connect(kwinService, testPath, testInterface, u"Swipe"_s, this, SLOT(onTestSwipe(int, QString)));
    } else {
        bus.disconnect(kwinService, testPath, testInterface, u"Fingers"_s, this, SLOT(onTestFingers(int)));
        bus.disconnect(kwinService, testPath, testInterface, u"Swipe"_s, this, SLOT(onTestSwipe(int, QString)));
    }
}

void SwyperKcm::startTest()
{
    if (m_testing) {
        return;
    }
    m_testError.clear();
    connectTestSignals(true);
    const QDBusPendingCall call =
        QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(kwinService, testPath, testInterface, u"Start"_s));
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError()) {
            connectTestSignals(false);
            m_testError = i18n("The gesture plugin isn't running yet. Log out and back in once after installing or updating Swyper.");
        } else {
            m_testing = true;
            m_testFingers = 0;
        }
        Q_EMIT testChanged();
    });
}

void SwyperKcm::stopTest()
{
    if (!m_testing) {
        return;
    }
    QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall(kwinService, testPath, testInterface, u"Stop"_s));
    connectTestSignals(false);
    m_testing = false;
    m_testFingers = 0;
    Q_EMIT testChanged();
}

void SwyperKcm::onTestFingers(int count)
{
    m_testFingers = count;
    Q_EMIT testChanged();
}

void SwyperKcm::onTestSwipe(int fingers, const QString &direction)
{
    Q_EMIT testSwipe(fingers, direction);
}

void SwyperKcm::load()
{
    m_config->reparseConfiguration();
    m_saved = tpg::Settings::load(*m_config);
    scanProfiles();
    replaceAll(m_saved);
}

void SwyperKcm::save()
{
    m_current.save(*m_config); // the plugin is notified and applies it at once
    m_saved = m_current;
    edited();
}

void SwyperKcm::defaults()
{
    replaceAll(tpg::Settings());
}

void SwyperKcm::applyPreset(const QString &id)
{
    tpg::Settings s = m_current;
    if (s.applyPreset(id)) {
        replaceAll(s);
    }
}

void SwyperKcm::replaceAll(const tpg::Settings &settings)
{
    m_current = settings; // the axis objects point into m_current, so they now show the new values
    for (AxisConfig *axis : m_axes) {
        Q_EMIT axis->changed();
    }
    Q_EMIT sensitivityChanged();
    edited();
}

void SwyperKcm::setSensitivity(double &field, double value)
{
    if (field == value) {
        return;
    }
    field = value;
    Q_EMIT sensitivityChanged();
    edited();
}

void SwyperKcm::setFingerChangeGrace(int v)
{
    if (m_current.fingerChangeGrace == v) {
        return;
    }
    m_current.fingerChangeGrace = v;
    Q_EMIT sensitivityChanged();
    edited();
}

void SwyperKcm::setIgnoreRestingThumb(bool v)
{
    if (m_current.ignoreRestingThumb == v) {
        return;
    }
    m_current.ignoreRestingThumb = v;
    Q_EMIT sensitivityChanged();
    edited();
}

void SwyperKcm::edited()
{
    setNeedsSave(!(m_current == m_saved));
    setRepresentsDefaults(m_current == tpg::Settings());
    Q_EMIT stateChanged();
}

QVariantList SwyperKcm::kdeActions()
{
    if (m_kdeActionsLoaded) {
        return m_kdeActions;
    }
    m_kdeActionsLoaded = true;

    const QString service = u"org.kde.kglobalaccel"_s;
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QDBusMessage components = bus.call(QDBusMessage::createMethodCall(service, u"/kglobalaccel"_s, u"org.kde.KGlobalAccel"_s, u"allComponents"_s));
    if (components.type() != QDBusMessage::ReplyMessage || components.arguments().isEmpty()) {
        return m_kdeActions;
    }

    struct Entry {
        QString target;
        QString label;
    };
    QList<Entry> entries;
    const auto paths = qdbus_cast<QList<QDBusObjectPath>>(components.arguments().constFirst());
    for (const QDBusObjectPath &path : paths) {
        const QString component = path.path().section(u'/', -1);
        const QDBusMessage reply =
            bus.call(QDBusMessage::createMethodCall(service, path.path(), u"org.kde.kglobalaccel.Component"_s, u"allShortcutInfos"_s));
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            continue;
        }
        // a(ssssssaiai): context id, context name, component id, component name, action id, action name, keys, default keys
        const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            QString contextId, contextName, componentId, componentName, actionId, actionName;
            QList<int> keys, defaultKeys;
            arg.beginStructure();
            arg >> contextId >> contextName >> componentId >> componentName >> actionId >> actionName >> keys >> defaultKeys;
            arg.endStructure();
            if (actionId.isEmpty() || contextId != u"default"_s) {
                continue;
            }
            const QString app = componentName.isEmpty() ? componentId : componentName;
            const QString name = actionName.isEmpty() ? actionId : actionName;
            entries.append({component + u'/' + actionId, app + u" › "_s + name});
        }
        arg.endArray();
    }

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(entries.begin(), entries.end(), [&collator](const Entry &a, const Entry &b) {
        return collator.compare(a.label, b.label) < 0;
    });
    for (const Entry &e : std::as_const(entries)) {
        m_kdeActions.append(QVariantMap{{u"target"_s, e.target}, {u"label"_s, e.label}});
    }
    return m_kdeActions;
}

QString SwyperKcm::kdeActionLabel(const QString &target)
{
    for (const QVariant &v : kdeActions()) {
        const QVariantMap m = v.toMap();
        if (m.value(u"target"_s).toString() == target) {
            return m.value(u"label"_s).toString();
        }
    }
    return {};
}

namespace
{
constexpr qint64 MaxProfileSize = 256 * 1024; // a profile is a few KB; refuse anything far bigger
constexpr int ProfileFormat = 1;

QString uniqueFileName(const QString &dir, const QString &name)
{
    QString slug;
    for (const QChar c : name.toLower()) {
        slug += c.isLetterOrNumber() ? c : u'-';
    }
    while (slug.contains(u"--"_s)) {
        slug.replace(u"--"_s, u"-"_s);
    }
    slug = slug.mid(slug.startsWith(u'-') ? 1 : 0).left(60);
    if (slug.endsWith(u'-')) {
        slug.chop(1);
    }
    if (slug.isEmpty()) {
        slug = u"profile"_s;
    }
    QString path = dir + u'/' + slug + u".swyper"_s;
    for (int i = 2; QFileInfo::exists(path); ++i) {
        path = dir + u'/' + slug + u'-' + QString::number(i) + u".swyper"_s;
    }
    return path;
}
}

QString SwyperKcm::profileDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/swyper/profiles"_s;
}

bool SwyperKcm::readProfile(const QString &path, Profile &out, QStringList *problems, QString *error) const
{
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable()) {
        *error = i18n("The file could not be read.");
        return false;
    }
    if (info.size() > MaxProfileSize) {
        *error = i18n("This file is too big to be a gestures profile.");
        return false;
    }
    const KConfig config(path, KConfig::SimpleConfig);
    const QStringList groups = config.groupList();
    if (!groups.contains(u"Profile"_s) && !groups.contains(u"Fingers3"_s) && !groups.contains(u"Fingers4"_s)) {
        *error = i18n("This file is not a gestures profile.");
        return false;
    }
    const KConfigGroup profile = config.group(u"Profile"_s);
    out.name = profile.readEntry("Name", QString()).trimmed().left(80);
    if (out.name.isEmpty()) {
        out.name = info.completeBaseName();
    }
    if (profile.readEntry("Format", ProfileFormat) > ProfileFormat && problems) {
        problems->append(i18n("This profile was made with a newer version of Swyper; some of its settings may not be used."));
    }
    out.path = path;
    out.settings = tpg::Settings::load(config, problems);
    return true;
}

QString SwyperKcm::writeProfile(const QString &path, const QString &name, const tpg::Settings &settings) const
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return i18n("The folder for the file could not be created.");
    }
    // Write a complete new file next to the target, then swap it in, so a half-written file never replaces a good one.
    const QString temp = path + u".new"_s;
    QFile::remove(temp);
    {
        KConfig config(temp, KConfig::SimpleConfig);
        settings.save(config, KConfig::Persistent);
        KConfigGroup profile = config.group(u"Profile"_s);
        profile.writeEntry("Name", name);
        profile.writeEntry("Format", ProfileFormat);
        profile.writeEntry("App", u"swyper"_s);
        if (!config.sync()) {
            QFile::remove(temp);
            return i18n("The file could not be written.");
        }
    }
    QFile::remove(path);
    if (!QFile::rename(temp, path)) {
        QFile::remove(temp);
        return i18n("The file could not be written.");
    }
    return {};
}

void SwyperKcm::scanProfiles()
{
    m_profiles.clear();
    const QDir dir(profileDir());
    for (const QString &file : dir.entryList({u"*.swyper"_s}, QDir::Files)) {
        Profile p;
        QString error;
        if (readProfile(dir.filePath(file), p, nullptr, &error)) {
            m_profiles.append(p);
        }
    }
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(m_profiles.begin(), m_profiles.end(), [&collator](const Profile &a, const Profile &b) {
        return collator.compare(a.name, b.name) < 0;
    });
    Q_EMIT profilesChanged();
}

QStringList SwyperKcm::profiles() const
{
    QStringList names;
    for (const Profile &p : m_profiles) {
        names.append(p.name);
    }
    return names;
}

QString SwyperKcm::activeProfile() const
{
    for (const Profile &p : m_profiles) {
        if (p.settings == m_current) {
            return p.name;
        }
    }
    return {};
}

bool SwyperKcm::profileExists(const QString &name) const
{
    return profiles().contains(name.trimmed());
}

void SwyperKcm::loadProfile(const QString &name)
{
    for (const Profile &p : std::as_const(m_profiles)) {
        if (p.name == name) {
            replaceAll(p.settings);
            return;
        }
    }
}

QString SwyperKcm::saveProfile(const QString &name)
{
    const QString clean = name.trimmed().left(80);
    if (clean.isEmpty()) {
        return i18n("Please enter a name for the profile.");
    }
    QString path;
    for (const Profile &p : std::as_const(m_profiles)) {
        if (p.name == clean) {
            path = p.path; // same name: replace that profile
        }
    }
    if (path.isEmpty()) {
        path = uniqueFileName(profileDir(), clean);
    }
    const QString error = writeProfile(path, clean, m_current);
    scanProfiles();
    Q_EMIT stateChanged();
    return error;
}

void SwyperKcm::deleteProfile(const QString &name)
{
    for (const Profile &p : std::as_const(m_profiles)) {
        if (p.name == name) {
            QFile::remove(p.path);
        }
    }
    scanProfiles();
    Q_EMIT stateChanged();
}

QVariantMap SwyperKcm::inspectFile(const QUrl &url)
{
    Profile p;
    QStringList problems;
    QString error;
    if (!url.isLocalFile() || !readProfile(url.toLocalFile(), p, &problems, &error)) {
        return {{u"ok"_s, false}, {u"error"_s, error.isEmpty() ? i18n("The file could not be read.") : error}};
    }
    // A profile can run commands; show every one of them before it is used.
    QStringList commands;
    for (const tpg::FingerSetting *f : {&p.settings.three, &p.settings.four}) {
        for (const tpg::AxisSetting *axis : {&f->vertical, &f->horizontal}) {
            if (axis->id != u"separate"_s) {
                continue;
            }
            for (const tpg::SingleAction *a : {&axis->first, &axis->second}) {
                if (a->id == u"command"_s && !a->target.trimmed().isEmpty()) {
                    commands.append(a->target);
                } else if (a->id == u"kde-action"_s && kdeActionLabel(a->target).isEmpty()) {
                    problems.append(i18n("The KDE action \"%1\" does not exist on this computer.", a->target));
                }
            }
        }
    }
    return {{u"ok"_s, true}, {u"name"_s, p.name}, {u"commands"_s, commands}, {u"problems"_s, problems}};
}

QString SwyperKcm::importFile(const QUrl &url, bool keepAsProfile)
{
    Profile p;
    QString error;
    if (!url.isLocalFile() || !readProfile(url.toLocalFile(), p, nullptr, &error)) {
        return error.isEmpty() ? i18n("The file could not be read.") : error;
    }
    replaceAll(p.settings);
    if (!keepAsProfile) {
        return {};
    }
    QString name = p.name;
    for (int i = 2; profileExists(name); ++i) {
        name = i18nc("profile name, number for a second copy", "%1 (%2)", p.name, i);
    }
    return saveProfile(name);
}

QString SwyperKcm::exportFile(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return i18n("Please choose a file on this computer.");
    }
    QString path = url.toLocalFile();
    if (!path.endsWith(u".swyper"_s)) {
        path += u".swyper"_s;
    }
    const QString active = activeProfile();
    return writeProfile(path, active.isEmpty() ? QFileInfo(path).completeBaseName() : active, m_current);
}

#include "kcm.moc"
