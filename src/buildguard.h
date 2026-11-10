// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Safety lock: the plugin reaches into KWin internals, which may change between KWin builds.
// It only starts when the running KWin is the exact build it was compiled against.
namespace BuildGuard
{
bool matchesRunningKWin();
void notifyPaused(); // log it and show a desktop notification once the notification service is up
}
