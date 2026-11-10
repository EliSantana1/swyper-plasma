// SPDX-License-Identifier: GPL-3.0-or-later
// Test mode for the settings page: while the page asks for it, swipes are reported over D-Bus
// (finger count, and the direction each swipe was decided in) instead of running their actions.
// It ends when the page stops it or its process goes away.
#pragma once

#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QString>

class TestService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.elisantana1.Swyper")
public:
    explicit TestService(QObject *parent = nullptr);
    ~TestService() override;

    bool active() const
    {
        return !m_client.isEmpty();
    }
    void reportFingers(int fingers);
    void reportSwipe(int fingers, const QString &direction);

public Q_SLOTS:
    Q_SCRIPTABLE void Start();
    Q_SCRIPTABLE void Stop();

Q_SIGNALS:
    Q_SCRIPTABLE void Fingers(int count);
    Q_SCRIPTABLE void Swipe(int fingers, const QString &direction); // direction: up, down, left, right

private:
    QString m_client; // D-Bus name of the page that started test mode
    QDBusServiceWatcher m_watcher;
    int m_lastFingers = -1;
};
