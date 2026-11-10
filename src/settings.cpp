// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"

#include <KConfigGroup>
#include <QStringList>
#include <algorithm>

using namespace Qt::StringLiterals;

namespace tpg
{
namespace
{
const QStringList axisActions = {u"plasma"_s, u"overview-grid"_s, u"desktops"_s, u"switcher"_s, u"volume"_s, u"brightness"_s, u"media"_s, u"separate"_s, u"none"_s};
const QStringList singleActions = {u"overview"_s, u"grid"_s, u"present-windows"_s, u"show-desktop"_s, u"play-pause"_s, u"mute"_s,
                                   u"screenshot"_s, u"krunner"_s, u"lock"_s, u"kde-action"_s, u"command"_s, u"none"_s};

void note(QStringList *problems, const QString &text)
{
    qWarning("swyper: %s", qPrintable(text));
    if (problems) {
        problems->append(text);
    }
}

SingleAction readSingle(const KConfigGroup &group, const SingleAction &fallback, QStringList *problems)
{
    SingleAction a;
    a.id = group.readEntry("Action", fallback.id);
    a.target = group.readEntry("Target", fallback.target);
    if (!singleActions.contains(a.id)) {
        note(problems, u"Unknown action \"%1\" in %2 was replaced by Nothing."_s.arg(a.id, group.name()));
        a = SingleAction();
    }
    return a;
}

void readAxis(const KConfigGroup &group, bool vertical, AxisSetting &axis, QStringList *problems)
{
    axis.id = group.readEntry("Action", axis.id);
    const bool valid = axisActions.contains(axis.id) && (vertical || axis.id != u"overview-grid"_s);
    if (!valid) {
        note(problems, u"Action \"%1\" is not possible in %2 and was replaced by Nothing."_s.arg(axis.id, group.name()));
        axis.id = u"none"_s;
    }
    axis.first = readSingle(group.group(vertical ? u"Up"_s : u"Left"_s), axis.first, problems);
    axis.second = readSingle(group.group(vertical ? u"Down"_s : u"Right"_s), axis.second, problems);
}
}

Settings::Settings()
{
    applyPreset(u"plasma"_s);
}

Settings Settings::load(const KConfig &config, QStringList *problems)
{
    Settings s;
    for (int n : {3, 4}) {
        FingerSetting &f = n == 3 ? s.three : s.four;
        const KConfigGroup fingers = config.group(u"Fingers%1"_s.arg(n));
        readAxis(fingers.group(u"Vertical"_s), true, f.vertical, problems);
        readAxis(fingers.group(u"Horizontal"_s), false, f.horizontal, problems);
    }
    const KConfigGroup sens = config.group(u"Sensitivity"_s);
    s.directionLock = std::clamp(sens.readEntry("DirectionLock", s.directionLock), 2.0, 100.0);
    s.switcherStep = std::clamp(sens.readEntry("SwitcherStep", s.switcherStep), 20.0, 600.0);
    s.stepDistance = std::clamp(sens.readEntry("StepDistance", s.stepDistance), 1.0, 100.0);
    s.triggerDistance = std::clamp(sens.readEntry("TriggerDistance", s.triggerDistance), 5.0, 400.0);
    s.fingerChangeGrace = std::clamp(sens.readEntry("FingerChangeGrace", s.fingerChangeGrace), 50, 1000);
    s.ignoreRestingThumb = config.group(u"Touchpad"_s).readEntry("IgnoreRestingThumb", s.ignoreRestingThumb);
    return s;
}

namespace
{
void writeSingle(KConfigGroup group, const SingleAction &a, KConfig::WriteConfigFlags flags)
{
    group.writeEntry("Action", a.id, flags);
    group.writeEntry("Target", a.target, flags);
}

void writeAxis(KConfigGroup group, bool vertical, const AxisSetting &axis, KConfig::WriteConfigFlags flags)
{
    group.writeEntry("Action", axis.id, flags);
    writeSingle(group.group(vertical ? u"Up"_s : u"Left"_s), axis.first, flags);
    writeSingle(group.group(vertical ? u"Down"_s : u"Right"_s), axis.second, flags);
}

AxisSetting axis(const QString &id)
{
    AxisSetting a;
    a.id = id;
    return a;
}

AxisSetting separate(const QString &first, const QString &second)
{
    AxisSetting a = axis(u"separate"_s);
    a.first.id = first;
    a.second.id = second;
    return a;
}
}

void Settings::save(KConfig &config, KConfig::WriteConfigFlags flags) const
{
    for (int n : {3, 4}) {
        const FingerSetting &f = n == 3 ? three : four;
        KConfigGroup fingers = config.group(u"Fingers%1"_s.arg(n));
        writeAxis(fingers.group(u"Vertical"_s), true, f.vertical, flags);
        writeAxis(fingers.group(u"Horizontal"_s), false, f.horizontal, flags);
    }
    KConfigGroup sens = config.group(u"Sensitivity"_s);
    sens.writeEntry("DirectionLock", directionLock, flags);
    sens.writeEntry("SwitcherStep", switcherStep, flags);
    sens.writeEntry("StepDistance", stepDistance, flags);
    sens.writeEntry("TriggerDistance", triggerDistance, flags);
    sens.writeEntry("FingerChangeGrace", fingerChangeGrace, flags);
    config.group(u"Touchpad"_s).writeEntry("IgnoreRestingThumb", ignoreRestingThumb, flags);
    config.sync();
}

bool Settings::applyPreset(const QString &id)
{
    if (id == u"windows"_s) { // Microsoft's touchpad gestures page; 4 up/down match 3 up/down
        three = {separate(u"overview"_s, u"show-desktop"_s), axis(u"switcher"_s)};
        four = {separate(u"overview"_s, u"show-desktop"_s), axis(u"desktops"_s)};
    } else if (id == u"macos"_s) { // Apple's Multi-Touch gestures page: four-finger swipes, no three-finger swipes
        three = {axis(u"none"_s), axis(u"none"_s)};
        four = {separate(u"overview"_s, u"present-windows"_s), axis(u"desktops"_s)};
    } else if (id == u"plasma"_s) { // stock Plasma: KWin's own gesture for every swipe
        three = {axis(u"plasma"_s), axis(u"plasma"_s)};
        four = {axis(u"plasma"_s), axis(u"plasma"_s)};
    } else {
        return false;
    }
    return true;
}

QString Settings::matchingPreset() const
{
    for (const QString &id : {u"windows"_s, u"macos"_s, u"plasma"_s}) {
        Settings p = *this;
        p.applyPreset(id);
        if (p.three == three && p.four == four) {
            return id;
        }
    }
    return {};
}
}
