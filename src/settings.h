// SPDX-License-Identifier: GPL-3.0-or-later
// The user's gesture settings, read from ~/.config/swyperrc.
// Every value has a default, so a missing or partial file gives Plasma's own behaviour.
#pragma once

#include <KConfig>
#include <QString>
#include <QStringList>

namespace tpg
{
// One action for one direction (used when an axis is set to "separate", and for media).
struct SingleAction {
    bool operator==(const SingleAction &) const = default;
    QString id = QStringLiteral("none"); // overview, grid, present-windows, show-desktop, play-pause, mute,
                                         // screenshot, krunner, lock, kde-action, command, none
    QString target; // kde-action: "component/action name"; command: the command line
};

// What one axis (up & down, or left & right) does for one finger count.
struct AxisSetting {
    bool operator==(const AxisSetting &) const = default;
    QString id = QStringLiteral("none"); // plasma (KWin's own gesture), overview-grid (vertical only), desktops,
                                         // switcher, volume, brightness, media, separate, none (does nothing)
    SingleAction first; // up or left (for "separate")
    SingleAction second; // down or right (for "separate")
};

struct FingerSetting {
    bool operator==(const FingerSetting &) const = default;
    AxisSetting vertical;
    AxisSetting horizontal;
};

struct Settings {
    FingerSetting three;
    FingerSetting four;
    double directionLock = 10.0; // movement before up/down vs left/right is decided
    double switcherStep = 120.0; // window switcher: slide per window
    double stepDistance = 6.0; // volume / brightness: slide per 1% step
    double triggerDistance = 60.0; // single actions: slide before the action runs
    int fingerChangeGrace = 250; // ms a 4-finger action survives libinput restarting the swipe
    bool ignoreRestingThumb = false; // leave a still contact at the bottom edge out of the finger count

    Settings(); // out of the box: Plasma's own gestures, so installing the app changes nothing
    bool operator==(const Settings &) const = default;
    const FingerSetting &forFingers(int fingers) const
    {
        return fingers >= 4 ? four : three;
    }
    // problems (optional) collects readable notes about entries that could not be used
    static Settings load(const KConfig &config, QStringList *problems = nullptr);
    // KConfig::Notify for the live settings, so the plugin reloads at once; KConfig::Persistent for profile files
    void save(KConfig &config, KConfig::WriteConfigFlags flags = KConfig::Notify) const;

    // Presets replace the gestures and keep the sensitivity: "windows", "macos", "plasma".
    bool applyPreset(const QString &id);
    QString matchingPreset() const; // the preset the gestures equal, or empty
};
}
