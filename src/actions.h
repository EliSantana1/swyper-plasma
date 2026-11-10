// SPDX-License-Identifier: GPL-3.0-or-later
// Runs actions through KDE's own global actions (KGlobalAccel), exactly as if their shortcut was used;
// nothing is faked with key presses.
#pragma once

#include "settings.h"

namespace tpg::actions
{
void run(const SingleAction &action);
void volume(bool up); // 1% step, with Plasma's volume popup
void brightness(bool up); // small step, with Plasma's brightness popup
}
