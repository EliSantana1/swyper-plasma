// SPDX-License-Identifier: GPL-3.0-or-later
#include "buildguard.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <cstring>
#include <elf.h>
#include <link.h>

namespace
{
// Build ID (hex) of the libkwin loaded in this process, read from its ELF notes in memory.
int findKWinBuildId(dl_phdr_info *info, size_t, void *data)
{
    if (!info->dlpi_name || !std::strstr(info->dlpi_name, "/libkwin.so")) {
        return 0;
    }
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) &ph = info->dlpi_phdr[i];
        if (ph.p_type != PT_NOTE) {
            continue;
        }
        const size_t align = ph.p_align >= 8 ? 8 : 4;
        const char *p = reinterpret_cast<const char *>(info->dlpi_addr + ph.p_vaddr);
        const char *end = p + ph.p_memsz;
        while (p + sizeof(ElfW(Nhdr)) <= end) {
            const auto *note = reinterpret_cast<const ElfW(Nhdr) *>(p);
            const char *name = p + sizeof(ElfW(Nhdr));
            const char *desc = name + ((note->n_namesz + align - 1) & ~(align - 1));
            if (desc + note->n_descsz > end) {
                break;
            }
            if (note->n_type == NT_GNU_BUILD_ID && note->n_namesz == 4 && std::memcmp(name, "GNU", 4) == 0) {
                *static_cast<QByteArray *>(data) = QByteArray(desc, note->n_descsz).toHex();
                return 1;
            }
            p = desc + ((note->n_descsz + align - 1) & ~(align - 1));
        }
    }
    return 0;
}

QByteArray runningBuildId()
{
    QByteArray id;
    dl_iterate_phdr(findKWinBuildId, &id);
    return id;
}
}

bool BuildGuard::matchesRunningKWin()
{
    return runningBuildId() == QByteArrayLiteral(TPG_KWIN_BUILD_ID);
}

void BuildGuard::notifyPaused()
{
    const QByteArray running = runningBuildId();
    qWarning("swyper: built for KWin build %s but KWin build %s is running - paused until it is rebuilt",
             TPG_KWIN_BUILD_ID, running.isEmpty() ? "(unknown)" : running.constData());

    static const QString service = QStringLiteral("org.freedesktop.Notifications");
    const auto send = [] {
        QDBusMessage msg = QDBusMessage::createMethodCall(service, QStringLiteral("/org/freedesktop/Notifications"), service, QStringLiteral("Notify"));
        msg << QStringLiteral("Swyper") << uint(0) << QStringLiteral("input-touchpad") << QStringLiteral("Swyper paused")
            << QStringLiteral("KDE Plasma was updated and Swyper has not been rebuilt for it yet. Plasma's standard gestures work until then.")
            << QStringList() << QVariantMap() << int(-1);
        QDBusConnection::sessionBus().asyncCall(msg);
    };
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.interface() && bus.interface()->isServiceRegistered(service)) {
        send();
        return;
    }
    // KWin starts before Plasma's notification service: wait for it to appear, send once, then clean up.
    auto *watcher = new QDBusServiceWatcher(service, bus, QDBusServiceWatcher::WatchForRegistration, QCoreApplication::instance());
    QObject::connect(watcher, &QDBusServiceWatcher::serviceRegistered, watcher, [watcher, send] {
        send();
        watcher->deleteLater();
    });
}
