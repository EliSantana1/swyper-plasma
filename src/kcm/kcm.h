// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KQuickConfigModule>
#include <KSharedConfig>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "settings.h"

class SwyperKcm;

// One axis (up & down, or left & right) of one finger count, as the page edits it.
// It points into the page's current settings, so every edit lands directly in them.
class AxisConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool vertical READ vertical CONSTANT)
    Q_PROPERTY(QString action READ action WRITE setAction NOTIFY changed)
    Q_PROPERTY(QString firstAction READ firstAction WRITE setFirstAction NOTIFY changed)
    Q_PROPERTY(QString firstTarget READ firstTarget WRITE setFirstTarget NOTIFY changed)
    Q_PROPERTY(QString secondAction READ secondAction WRITE setSecondAction NOTIFY changed)
    Q_PROPERTY(QString secondTarget READ secondTarget WRITE setSecondTarget NOTIFY changed)
public:
    AxisConfig(tpg::AxisSetting *data, bool vertical, SwyperKcm *kcm);

    bool vertical() const { return m_vertical; }
    QString action() const { return m_data->id; }
    QString firstAction() const { return m_data->first.id; }
    QString firstTarget() const { return m_data->first.target; }
    QString secondAction() const { return m_data->second.id; }
    QString secondTarget() const { return m_data->second.target; }
    void setAction(const QString &v) { set(m_data->id, v); }
    void setFirstAction(const QString &v) { set(m_data->first.id, v); }
    void setFirstTarget(const QString &v) { set(m_data->first.target, v); }
    void setSecondAction(const QString &v) { set(m_data->second.id, v); }
    void setSecondTarget(const QString &v) { set(m_data->second.target, v); }

Q_SIGNALS:
    void changed();

private:
    void set(QString &field, const QString &value);
    tpg::AxisSetting *m_data;
    bool m_vertical;
    SwyperKcm *m_kcm;
};

class SwyperKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(QObject *threeVertical READ threeVertical CONSTANT)
    Q_PROPERTY(QObject *threeHorizontal READ threeHorizontal CONSTANT)
    Q_PROPERTY(QObject *fourVertical READ fourVertical CONSTANT)
    Q_PROPERTY(QObject *fourHorizontal READ fourHorizontal CONSTANT)
    Q_PROPERTY(double directionLock READ directionLock WRITE setDirectionLock NOTIFY sensitivityChanged)
    Q_PROPERTY(double switcherStep READ switcherStep WRITE setSwitcherStep NOTIFY sensitivityChanged)
    Q_PROPERTY(double stepDistance READ stepDistance WRITE setStepDistance NOTIFY sensitivityChanged)
    Q_PROPERTY(double triggerDistance READ triggerDistance WRITE setTriggerDistance NOTIFY sensitivityChanged)
    Q_PROPERTY(int fingerChangeGrace READ fingerChangeGrace WRITE setFingerChangeGrace NOTIFY sensitivityChanged)
    Q_PROPERTY(bool ignoreRestingThumb READ ignoreRestingThumb WRITE setIgnoreRestingThumb NOTIFY sensitivityChanged)
    Q_PROPERTY(QString activePreset READ activePreset NOTIFY stateChanged)
    Q_PROPERTY(QStringList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString activeProfile READ activeProfile NOTIFY stateChanged)
    Q_PROPERTY(bool testing READ testing NOTIFY testChanged)
    Q_PROPERTY(int testFingers READ testFingers NOTIFY testChanged)
    Q_PROPERTY(QString testError READ testError NOTIFY testChanged)
public:
    SwyperKcm(QObject *parent, const KPluginMetaData &data);
    ~SwyperKcm() override;

    void load() override;
    void save() override;
    void defaults() override;

    QObject *threeVertical() const { return m_axes[0]; }
    QObject *threeHorizontal() const { return m_axes[1]; }
    QObject *fourVertical() const { return m_axes[2]; }
    QObject *fourHorizontal() const { return m_axes[3]; }

    double directionLock() const { return m_current.directionLock; }
    double switcherStep() const { return m_current.switcherStep; }
    double stepDistance() const { return m_current.stepDistance; }
    double triggerDistance() const { return m_current.triggerDistance; }
    int fingerChangeGrace() const { return m_current.fingerChangeGrace; }
    void setDirectionLock(double v) { setSensitivity(m_current.directionLock, v); }
    void setSwitcherStep(double v) { setSensitivity(m_current.switcherStep, v); }
    void setStepDistance(double v) { setSensitivity(m_current.stepDistance, v); }
    void setTriggerDistance(double v) { setSensitivity(m_current.triggerDistance, v); }
    void setFingerChangeGrace(int v);
    bool ignoreRestingThumb() const { return m_current.ignoreRestingThumb; }
    void setIgnoreRestingThumb(bool v);

    QString activePreset() const { return m_current.matchingPreset(); }
    Q_INVOKABLE void applyPreset(const QString &id);

    // KDE's global actions (KGlobalAccel), as {target: "component/action", label: "Component › Action"}
    Q_INVOKABLE QVariantList kdeActions();
    Q_INVOKABLE QString kdeActionLabel(const QString &target); // empty when the action doesn't exist

    // Profiles: one self-contained .swyper file each, in ~/.local/share/swyper/profiles.
    // The same file format is used to share a profile with others (export / import).
    QStringList profiles() const;
    QString activeProfile() const; // the saved profile the page currently equals, or empty
    Q_INVOKABLE bool profileExists(const QString &name) const;
    Q_INVOKABLE void loadProfile(const QString &name); // into the page; Apply makes it active
    Q_INVOKABLE QString saveProfile(const QString &name); // the page's settings; returns an error or ""
    Q_INVOKABLE void deleteProfile(const QString &name);
    // {ok, error, name, commands, problems}: what a file would do, shown before importing it
    Q_INVOKABLE QVariantMap inspectFile(const QUrl &url);
    Q_INVOKABLE QString importFile(const QUrl &url, bool keepAsProfile); // returns an error or ""
    Q_INVOKABLE QString exportFile(const QUrl &url); // returns an error or ""

    // Test mode: the plugin reports swipes here instead of running them, while this page is open.
    bool testing() const { return m_testing; }
    int testFingers() const { return m_testFingers; }
    QString testError() const { return m_testError; }
    Q_INVOKABLE void startTest();
    Q_INVOKABLE void stopTest();

    void edited(); // something changed: update Apply / Defaults

Q_SIGNALS:
    void sensitivityChanged();
    void stateChanged();
    void profilesChanged();
    void testChanged();
    void testSwipe(int fingers, const QString &direction);

private Q_SLOTS:
    void onTestFingers(int count);
    void onTestSwipe(int fingers, const QString &direction);

private:
    void setSensitivity(double &field, double value);
    void replaceAll(const tpg::Settings &settings);

    struct Profile {
        QString name;
        QString path;
        tpg::Settings settings;
    };
    static QString profileDir();
    bool readProfile(const QString &path, Profile &out, QStringList *problems, QString *error) const;
    QString writeProfile(const QString &path, const QString &name, const tpg::Settings &settings) const;
    void scanProfiles();
    QList<Profile> m_profiles;
    void connectTestSignals(bool connect);
    bool m_testing = false;
    int m_testFingers = 0;
    QString m_testError;

    KSharedConfig::Ptr m_config;
    tpg::Settings m_saved;
    tpg::Settings m_current;
    AxisConfig *m_axes[4];
    QVariantList m_kdeActions;
    bool m_kdeActionsLoaded = false;
};
