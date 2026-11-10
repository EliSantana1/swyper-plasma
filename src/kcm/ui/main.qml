// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: root

    property string lastSwipe: ""

    Component.onDestruction: kcm.stopTest()

    actions: [
        Kirigami.Action {
            text: kcm.testing ? i18n("Stop Testing") : i18n("Test Gestures")
            icon.name: kcm.testing ? "media-playback-stop" : "input-touchpad"
            tooltip: i18n("Shows what your swipes would do with the settings on this page, without running them")
            onTriggered: {
                root.lastSwipe = "";
                kcm.testing ? kcm.stopTest() : kcm.startTest();
            }
        }
    ]

    header: ColumnLayout {
        spacing: 0
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: kcm.testing
            type: Kirigami.MessageType.Information
            icon.name: "input-touchpad"
            text: (root.lastSwipe !== "" ? root.lastSwipe : i18n("Testing: swipe on your touchpad. Nothing runs while testing."))
                  + "\n" + i18np("%1 finger on the touchpad", "%1 fingers on the touchpad", kcm.testFingers)
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            visible: kcm.testError !== ""
            type: Kirigami.MessageType.Error
            text: kcm.testError
        }
    }

    function textFor(list, value) {
        for (const item of list) {
            if (item.value === value) {
                return item.text;
            }
        }
        return value;
    }

    // What a swipe would do with the settings on this page (also before Apply)
    function describeSwipe(fingers, direction) {
        const vertical = direction === "up" || direction === "down";
        const firstSide = direction === "up" || direction === "left";
        const axis = fingers >= 4 ? (vertical ? kcm.fourVertical : kcm.fourHorizontal)
                                  : (vertical ? kcm.threeVertical : kcm.threeHorizontal);
        let action;
        if (axis.action === "separate") {
            const id = firstSide ? axis.firstAction : axis.secondAction;
            const target = firstSide ? axis.firstTarget : axis.secondTarget;
            action = textFor(singleActions, id);
            if (id === "command" && target !== "") {
                action = i18n("Run “%1”", target);
            } else if (id === "kde-action" && target !== "") {
                action = kcm.kdeActionLabel(target) || target;
            }
        } else if (axis.action === "media") {
            action = firstSide ? i18n("Previous track") : i18n("Next track");
        } else {
            action = textFor(vertical ? verticalActions : horizontalActions, axis.action);
        }
        const names = { up: i18n("up"), down: i18n("down"), left: i18n("left"), right: i18n("right") };
        return i18np("%1 finger, swipe %2: %3", "%1 fingers, swipe %2: %3", fingers, names[direction], action);
    }

    Connections {
        target: kcm
        function onTestSwipe(fingers, direction) {
            root.lastSwipe = root.describeSwipe(fingers, direction);
        }
    }

    // ids must match the plugin's action names (settings.cpp)
    readonly property var verticalActions: [
        { value: "plasma", text: i18n("Plasma's built-in gesture"), follows: true,
          hint: i18n("Whatever Plasma itself does with this swipe, with its own animation.") },
        { value: "overview-grid", text: i18n("Overview (up) · Grid (down)"), follows: true,
          hint: i18n("KWin's Overview and Grid. The animation follows your fingers; lift to finish or slide back to cancel.") },
        { value: "desktops", text: i18n("Switch virtual desktop"), follows: true,
          hint: i18n("Slides between virtual desktops that are stacked above each other.") },
        { value: "switcher", text: i18n("Window switcher"),
          hint: i18n("Slide to move through open windows, lift your fingers to switch.") },
        { value: "volume", text: i18n("Volume up / down"),
          hint: i18n("Changes the volume in 1% steps and shows Plasma's volume popup.") },
        { value: "brightness", text: i18n("Screen brightness up / down"),
          hint: i18n("Changes the screen brightness in small steps and shows Plasma's brightness popup.") },
        { value: "media", text: i18n("Previous / next track"),
          hint: i18n("Up plays the previous track, down the next one, in whatever is playing.") },
        { value: "separate", text: i18n("A different action for up and down…"),
          hint: i18n("Pick one action for each direction below.") },
        { value: "none", text: i18n("Nothing"), hint: i18n("This swipe does nothing.") }
    ]
    readonly property var horizontalActions: [
        { value: "plasma", text: i18n("Plasma's built-in gesture"), follows: true,
          hint: i18n("Whatever Plasma itself does with this swipe, with its own animation.") },
        { value: "switcher", text: i18n("Window switcher (Alt+Tab style)"),
          hint: i18n("Slide to move through open windows, lift your fingers to switch. Windows 11 style.") },
        { value: "desktops", text: i18n("Switch virtual desktop"), follows: true,
          hint: i18n("KWin's own desktop slide. The animation follows your fingers.") },
        { value: "media", text: i18n("Previous / next track"),
          hint: i18n("Left plays the previous track, right the next one, in whatever is playing.") },
        { value: "volume", text: i18n("Volume down / up"),
          hint: i18n("Changes the volume in 1% steps and shows Plasma's volume popup.") },
        { value: "brightness", text: i18n("Screen brightness down / up"),
          hint: i18n("Changes the screen brightness in small steps and shows Plasma's brightness popup.") },
        { value: "separate", text: i18n("A different action for left and right…"),
          hint: i18n("Pick one action for each direction below.") },
        { value: "none", text: i18n("Nothing"), hint: i18n("This swipe does nothing.") }
    ]
    readonly property var singleActions: [
        { value: "overview", text: i18n("Overview") },
        { value: "grid", text: i18n("Grid (all desktops)") },
        { value: "present-windows", text: i18n("Windows of current app") },
        { value: "show-desktop", text: i18n("Show desktop") },
        { value: "play-pause", text: i18n("Play / pause media") },
        { value: "mute", text: i18n("Mute / unmute") },
        { value: "screenshot", text: i18n("Screenshot of a region") },
        { value: "krunner", text: i18n("Open KRunner (search)") },
        { value: "lock", text: i18n("Lock screen") },
        { value: "kde-action", text: i18n("Run a KDE action…") },
        { value: "command", text: i18n("Run a command…") },
        { value: "none", text: i18n("Nothing") }
    ]
    readonly property var presets: [
        { id: "windows", text: i18n("Windows 11 Style") },
        { id: "macos", text: i18n("macOS Style") },
        { id: "plasma", text: i18n("Plasma Default") }
    ]

    ColumnLayout {
        // a page's content is only as wide as it needs to be unless told otherwise; use the full width
        width: root.availableWidth
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("Choose what 3- and 4-finger swipes do. Changes take effect as soon as you press Apply.")
        }

        Kirigami.InlineMessage {
            id: status
            Layout.fillWidth: true
            showCloseButton: true
            visible: false
        }

        ProfileBar {
            Layout.fillWidth: true
            availableWidth: root.width
            onMessage: (text, type) => {
                status.text = text;
                status.type = type;
                status.visible = true;
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing
            QQC2.Label {
                height: presetButtons.height
                verticalAlignment: Text.AlignVCenter
                text: i18n("Start from a preset:")
            }
            Row {
                id: presetButtons
                spacing: Kirigami.Units.smallSpacing
                Repeater {
                    model: root.presets
                    delegate: QQC2.Button {
                        required property var modelData
                        text: modelData.text
                        highlighted: kcm.activePreset === modelData.id
                        onClicked: kcm.applyPreset(modelData.id)
                    }
                }
            }
        }

        QQC2.TabBar {
            id: fingerTabs
            Layout.fillWidth: true
            QQC2.TabButton { text: i18n("3 Fingers") }
            QQC2.TabButton { text: i18n("4 Fingers") }
        }

        GestureRow {
            Layout.fillWidth: true
            title: i18n("Swipe up & down")
            axis: fingerTabs.currentIndex === 0 ? kcm.threeVertical : kcm.fourVertical
            actions: root.verticalActions
            singles: root.singleActions
            firstLabel: i18n("Up")
            secondLabel: i18n("Down")
        }

        GestureRow {
            Layout.fillWidth: true
            title: i18n("Swipe left & right")
            axis: fingerTabs.currentIndex === 0 ? kcm.threeHorizontal : kcm.fourHorizontal
            actions: root.horizontalActions
            singles: root.singleActions
            firstLabel: i18n("Left")
            secondLabel: i18n("Right")
        }

        RowLayout {
            Layout.fillWidth: true
            Item {
                Layout.fillWidth: true
            }
            QQC2.Button {
                text: i18n("Advanced Settings…")
                icon.name: "configure"
                onClicked: advanced.open()
            }
        }
    }

    AdvancedDialog {
        id: advanced
        objectName: "advancedDialog"
    }
}
