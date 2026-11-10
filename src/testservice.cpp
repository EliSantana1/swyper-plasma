// SPDX-License-Identifier: GPL-3.0-or-later
#include "testservice.h"

#include <QDBusConnection>
#include <QDBusMessage>

using namespace Qt::StringLiterals;

namespace
{
const QString path = u"/Swyper"_s;
}

TestService::TestService(QObject *parent)
    : QObject(parent)
    , m_watcher(QString(), QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForUnregistration)
{
    connect(&m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &name) {
        if (name == m_client) {
            Stop(); // the settings page closed or crashed without stopping test mode
        }
    });
    QDBusConnection::sessionBus().registerObject(path, this, QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);
}

TestService::~TestService()
{
    QDBusConnection::sessionBus().unregisterObject(path);
}

void TestService::Start()
{
    if (!calledFromDBus()) {
        return;
    }
    Stop();
    m_client = message().service();
    m_watcher.addWatchedService(m_client);
    m_lastFingers = -1;
    qInfo("swyper: test mode on");
}

void TestService::Stop()
{
    if (m_client.isEmpty()) {
        return;
    }
    m_watcher.removeWatchedService(m_client);
    m_client.clear();
    qInfo("swyper: test mode off");
}

void TestService::reportFingers(int fingers)
{
    if (active() && fingers != m_lastFingers) {
        m_lastFingers = fingers;
        Q_EMIT Fingers(fingers);
    }
}

void TestService::reportSwipe(int fingers, const QString &direction)
{
    if (active()) {
        Q_EMIT Swipe(fingers, direction);
    }
}
