// SPDX-License-Identifier: GPL-3.0-or-later
// Swyper: a KWin plugin for configurable 3- and 4-finger touchpad swipes.
// - Settings live in ~/.config/swyperrc and take effect the moment they are saved (KConfigWatcher).
// - Overview/Grid and desktop switching are fed into KWin's own gesture engine, so their animations follow
//   the fingers. The window switcher (Alt+Tab style) is driven directly: slide to pick, lift to switch.
//   Volume, brightness, media and other actions run KDE's own global actions; no key presses are faked.
// - Finger count: libinput only admits a 4th finger 100 ms after it lands (then restarts the swipe), and it
//   ignores fingers it takes for a thumb or palm. So on every swipe event the plugin also asks the touchpad
//   hardware how many fingers are down (touchcount.h) and switches to the 4-finger action the moment a 4th
//   finger touches, carrying it through libinput's restart.
// It sits in KWin's input chain before KWin's global-shortcut gestures.

#include <QPointF>
#include <QTimer>
#include <cmath>
#include <memory>

#include <KConfigWatcher>
#include <KSharedConfig>

#include "actions.h"
#include "buildguard.h"
#include "settings.h"
#include "testservice.h"
#include "touchcount.h"

#include <kwin/effect/effecthandler.h>
#include <kwin/gestures.h>
#include <kwin/input.h>
#include <kwin/input_event.h>
#include <kwin/plugin.h>
// KWin keeps its touchpad gesture engine private; reach it the same way KWin itself is built.
#define private public
#include <kwin/globalshortcuts.h>
#undef private
#include <kwin/effect/globals.h>
#include <kwin/workspace.h>

// KWin's window switcher (Alt+Tab). Its header isn't installed, but these methods are exported by KWin.
namespace KWin::TabBox
{
class TabBox
{
public:
    bool toggleMode(TabBoxMode mode); // open the switcher without needing a key held
    void nextPrev(bool next); // move the highlight one window
    void accept(bool closeTabBox = true); // switch to the highlighted window
    void close(bool abort = false);
};
}

using namespace KWin;
using namespace Qt::StringLiterals;

namespace
{
TabBox::TabBox *switcher()
{
    return workspace() ? workspace()->tabbox() : nullptr;
}

GestureRecognizer *recognizer()
{
    if (!input() || !input()->shortcuts()) {
        return nullptr;
    }
    return input()->shortcuts()->m_touchpadGestureRecognizer.get();
}

class SwipeFilter : public InputEventFilter
{
public:
    SwipeFilter(const tpg::Settings &settings, TestService *test)
        : InputEventFilter(InputFilterOrder::TabBox) // before GlobalShortcut, where KWin's own gestures are handled
        , m_test(test)
    {
        m_carry.setSingleShot(true);
        QObject::connect(&m_carry, &QTimer::timeout, [this] {
            finish(true); // libinput never resumed the swipe: the fingers were lifted
        });
        setSettings(settings);
    }

    // New settings apply from the next swipe; one in progress keeps the action it started with.
    void setSettings(const tpg::Settings &settings)
    {
        m_settings = settings;
        m_carry.setInterval(settings.fingerChangeGrace);
    }

    bool swipeGestureBegin(PointerSwipeGestureBeginEvent *event) override
    {
        const int hw = m_touch.fingers(m_settings.ignoreRestingThumb);
        const int fingers = (hw >= 4 || event->fingerCount >= 4) ? 4 : event->fingerCount;
        m_test->reportFingers(std::max(hw, int(event->fingerCount)));
        if (m_carry.isActive()) {
            m_carry.stop();
            if (fingers == 4 && m_fingers == 4) {
                return true; // libinput restarted the swipe after counting the 4th finger: carry on
            }
            finish(true);
        }
        m_state = Idle;
        if (fingers != 3 && fingers != 4) {
            return false;
        }
        m_fingers = fingers;
        m_state = Deciding;
        m_total = QPointF();
        return true; // hold it back from KWin until the direction is known
    }

    bool swipeGestureUpdate(PointerSwipeGestureUpdateEvent *event) override
    {
        if (m_state == Idle) {
            return false;
        }
        if (m_test->active()) {
            m_test->reportFingers(std::max(m_touch.fingers(m_settings.ignoreRestingThumb), m_fingers));
        }
        if (m_fingers == 3 && m_touch.fingers(m_settings.ignoreRestingThumb) >= 4) {
            upgradeToFour();
            if (m_state == Idle) {
                return false;
            }
        }
        const double along = alongAxis(event->delta);
        switch (m_state) {
        case Idle:
            return false;
        case Done:
            return true;
        case Native:
            if (auto r = recognizer()) {
                r->updateSwipeGesture(event->delta);
            }
            return true;
        case Stepping:
            step(along);
            return true;
        case Single:
            m_single += along;
            fireSingleIfFar();
            return true;
        case Switching:
            // one window per full step of sliding, counted from where the last step happened; leftover
            // movement is dropped, so small wobbles or the drift of lifting the fingers never add an extra step
            m_step += along;
            if (std::abs(m_step) >= m_settings.switcherStep) {
                const bool forward = m_step > 0;
                m_step = 0;
                if (auto s = switcher()) {
                    s->nextPrev(forward);
                }
            }
            return true;
        case Deciding:
            m_total += event->delta;
            if (std::hypot(m_total.x(), m_total.y()) >= m_settings.directionLock) {
                decide();
            }
            return true;
        }
        return false;
    }

    bool swipeGestureEnd(PointerSwipeGestureEndEvent *) override
    {
        if (m_carry.isActive()) {
            m_carry.stop();
        }
        m_test->reportFingers(0);
        return finish(true);
    }

    bool swipeGestureCancelled(PointerSwipeGestureCancelEvent *) override
    {
        if (m_state == Idle) {
            return false;
        }
        // libinput cancels the swipe when the finger count changes and then starts a new one;
        // while 4 fingers are still down, keep the 4-finger action going through that restart.
        if (m_fingers == 4 && m_touch.fingers(m_settings.ignoreRestingThumb) >= 4) {
            m_carry.start();
            return true;
        }
        return finish(false);
    }

private:
    enum State { Idle, Deciding, Native, Switching, Stepping, Single, Done };

    double alongAxis(const QPointF &delta) const
    {
        return m_vertical ? delta.y() : delta.x(); // negative = up or left
    }

    void decide()
    {
        m_vertical = std::abs(m_total.y()) > std::abs(m_total.x());
        const tpg::FingerSetting &f = m_settings.forFingers(m_fingers);
        m_axis = m_vertical ? f.vertical : f.horizontal; // a copy: settings may be reloaded mid-swipe
        const double along = alongAxis(m_total);
        const QString &id = m_axis.id;

        if (m_test->active()) {
            // test mode: tell the settings page what was swiped; it shows what the page's settings would do
            m_state = Done;
            m_test->reportSwipe(m_fingers, m_vertical ? (along < 0 ? u"up"_s : u"down"_s) : (along < 0 ? u"left"_s : u"right"_s));
            return;
        }

        if (id == u"plasma"_s || id == u"overview-grid"_s || id == u"desktops"_s) {
            // KWin's own gestures, animation following the fingers. "plasma" hands over the real finger count, so
            // KWin does whatever it does itself; Overview/Grid are its 4-finger up/down, desktops its 3-finger
            // up/down (vertical desktop layouts) and 4-finger left/right.
            uint fingers = 4;
            if (id == u"plasma"_s) {
                fingers = m_fingers;
            } else if (id == u"desktops"_s && m_vertical) {
                fingers = 3;
            }
            if (auto r = recognizer()) {
                m_state = Native;
                r->startSwipeGesture(fingers);
                r->updateSwipeGesture(m_total);
            } else {
                m_state = Done;
            }
        } else if (id == u"switcher"_s) {
            auto s = switcher();
            if (s && s->toggleMode(TabBoxWindowsMode)) {
                m_state = Switching;
                m_step = 0;
                s->nextPrev(along > 0); // first step: the previous window
            } else {
                m_state = Done;
            }
        } else if (id == u"volume"_s || id == u"brightness"_s) {
            m_state = Stepping;
            m_step = 0;
            step(along);
        } else if (id == u"media"_s || id == u"separate"_s) {
            if (id == u"media"_s) {
                m_axis.first = {u"previous-track"_s, {}};
                m_axis.second = {u"next-track"_s, {}};
            }
            m_state = Single;
            m_single = along;
            fireSingleIfFar();
        } else {
            m_state = Done; // "none": this swipe does nothing
        }
    }

    void fireSingleIfFar()
    {
        if (std::abs(m_single) < m_settings.triggerDistance) {
            return;
        }
        m_state = Done; // once per swipe
        tpg::actions::run(m_single < 0 ? m_axis.first : m_axis.second);
    }

    // volume / brightness: up or right = more, one small step per stepDistance of sliding
    void step(double along)
    {
        const bool volume = m_axis.id == u"volume"_s;
        m_step += m_vertical ? -along : along;
        while (m_step >= m_settings.stepDistance) {
            m_step -= m_settings.stepDistance;
            volume ? tpg::actions::volume(true) : tpg::actions::brightness(true);
        }
        while (m_step <= -m_settings.stepDistance) {
            m_step += m_settings.stepDistance;
            volume ? tpg::actions::volume(false) : tpg::actions::brightness(false);
        }
    }

    // a 4th finger landed during a 3-finger swipe: undo the 3-finger action, start deciding as 4 fingers
    void upgradeToFour()
    {
        if (m_state == Native) {
            if (auto r = recognizer()) {
                r->cancelSwipeGesture();
            }
        } else if (m_state == Switching) {
            if (auto sw = switcher()) {
                sw->close(true);
            }
        }
        m_fingers = 4;
        m_state = Deciding;
        m_total = QPointF();
    }

    // end the current action; ok = fingers lifted normally (commit), !ok = cancelled
    bool finish(bool ok)
    {
        const State s = m_state;
        m_state = Idle;
        if (s == Native) {
            if (auto r = recognizer()) {
                if (ok) {
                    r->endSwipeGesture();
                } else {
                    r->cancelSwipeGesture();
                }
            }
        } else if (s == Switching) {
            if (auto sw = switcher()) {
                if (ok) {
                    sw->accept(true); // switch to the highlighted window
                } else {
                    sw->close(true);
                }
            }
        }
        return s != Idle;
    }

    tpg::Settings m_settings;
    tpg::AxisSetting m_axis;
    State m_state = Idle;
    bool m_vertical = false;
    double m_step = 0;
    double m_single = 0;
    int m_fingers = 0;
    QPointF m_total;
    QTimer m_carry;
    tpg::TouchCount m_touch;
    TestService *m_test;
};

class SwyperPlugin : public Plugin
{
    Q_OBJECT
public:
    SwyperPlugin()
    {
        if (!BuildGuard::matchesRunningKWin()) {
            BuildGuard::notifyPaused(); // built for another KWin: stay out of its internals, Plasma's own gestures stay active
            return;
        }
        m_config = KSharedConfig::openConfig(u"swyperrc"_s, KConfig::NoGlobals);
        m_test = new TestService(this);
        m_filter = std::make_unique<SwipeFilter>(tpg::Settings::load(*m_config), m_test);

        // Saved settings arrive as one change notification per group: reload once for all of them.
        m_reload.setSingleShot(true);
        m_reload.setInterval(0);
        connect(&m_reload, &QTimer::timeout, this, [this] {
            m_filter->setSettings(tpg::Settings::load(*m_config));
            qInfo("swyper: settings reloaded");
        });
        m_watcher = KConfigWatcher::create(m_config);
        connect(m_watcher.data(), &KConfigWatcher::configChanged, &m_reload, qOverload<>(&QTimer::start));

        input()->installInputEventFilter(m_filter.get());
        qInfo("swyper: loaded (gesture engine %s)", recognizer() ? "found" : "NOT found");
    }
    ~SwyperPlugin() override
    {
        if (m_filter && input()) {
            input()->uninstallInputEventFilter(m_filter.get());
        }
    }

private:
    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_watcher;
    QTimer m_reload;
    TestService *m_test = nullptr;
    std::unique_ptr<SwipeFilter> m_filter;
};

class SwyperFactory : public PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)
public:
    std::unique_ptr<Plugin> create() const override
    {
        return std::make_unique<SwyperPlugin>();
    }
};
}

#include "swyper.moc"
