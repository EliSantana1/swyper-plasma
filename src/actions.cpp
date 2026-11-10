// SPDX-License-Identifier: GPL-3.0-or-later
#include "actions.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QHash>
#include <QProcess>
#include <utility>

using namespace Qt::StringLiterals;

namespace tpg::actions
{
namespace
{
// Fire-and-forget: KDE's global shortcut service lives inside KWin itself, so waiting for a reply would block KWin.
void invokeShortcut(const QString &component, const QString &name)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(u"org.kde.kglobalaccel"_s, u"/component/"_s + component, u"org.kde.kglobalaccel.Component"_s, u"invokeShortcut"_s);
    msg << name;
    QDBusConnection::sessionBus().send(msg);
}

const QHash<QString, std::pair<QString, QString>> &builtIns()
{
    static const QHash<QString, std::pair<QString, QString>> table = {
        {u"overview"_s, {u"kwin"_s, u"Overview"_s}},
        {u"grid"_s, {u"kwin"_s, u"Grid View"_s}},
        {u"present-windows"_s, {u"kwin"_s, u"ExposeClass"_s}},
        {u"show-desktop"_s, {u"kwin"_s, u"Show Desktop"_s}},
        {u"play-pause"_s, {u"mediacontrol"_s, u"playpausemedia"_s}},
        {u"mute"_s, {u"kmix"_s, u"mute"_s}},
        {u"screenshot"_s, {u"org_kde_spectacle_desktop"_s, u"RectangularRegionScreenShot"_s}},
        {u"krunner"_s, {u"org_kde_krunner_desktop"_s, u"_launch"_s}},
        {u"lock"_s, {u"ksmserver"_s, u"Lock Session"_s}},
        {u"previous-track"_s, {u"mediacontrol"_s, u"previousmedia"_s}},
        {u"next-track"_s, {u"mediacontrol"_s, u"nextmedia"_s}},
    };
    return table;
}
}

void run(const SingleAction &action)
{
    if (action.id == u"none"_s) {
        return;
    }
    if (action.id == u"command"_s) {
        if (!action.target.trimmed().isEmpty()) {
            QProcess::startDetached(u"/bin/sh"_s, {u"-c"_s, action.target});
        }
        return;
    }
    if (action.id == u"kde-action"_s) {
        const qsizetype slash = action.target.indexOf(u'/');
        if (slash > 0) {
            invokeShortcut(action.target.left(slash), action.target.mid(slash + 1));
        } else {
            qWarning("swyper: KDE action \"%s\" is not in the form component/action", qPrintable(action.target));
        }
        return;
    }
    const auto it = builtIns().constFind(action.id);
    if (it != builtIns().constEnd()) {
        invokeShortcut(it->first, it->second);
    }
}

void volume(bool up)
{
    invokeShortcut(u"kmix"_s, up ? u"increase_volume_small"_s : u"decrease_volume_small"_s);
}

void brightness(bool up)
{
    invokeShortcut(u"org_kde_powerdevil"_s, up ? u"Increase Screen Brightness Small"_s : u"Decrease Screen Brightness Small"_s);
}
}
