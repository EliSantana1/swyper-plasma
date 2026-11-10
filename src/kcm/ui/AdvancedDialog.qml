// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Tuning options most people never need: sensitivity and finger detection.
Kirigami.Dialog {
    id: dialog

    parent: QQC2.Overlay.overlay // centred over the window
    title: i18n("Advanced Settings")
    standardButtons: Kirigami.Dialog.Close
    padding: Kirigami.Units.largeSpacing
    preferredWidth: Kirigami.Units.gridUnit * 42

    Kirigami.FormLayout {
        LabeledSlider {
            Kirigami.FormData.label: i18n("Direction lock distance:")
            from: 4; to: 40; unit: i18n("units")
            value: kcm.directionLock
            hint: i18n("How far your fingers move before up/down or left/right is decided.")
            onEdited: newValue => kcm.directionLock = newValue
        }
        LabeledSlider {
            Kirigami.FormData.label: i18n("Window switcher, slide per window:")
            from: 40; to: 300; stepSize: 10; unit: i18n("units")
            value: kcm.switcherStep
            hint: i18n("Bigger means more sliding to move to the next window.")
            onEdited: newValue => kcm.switcherStep = newValue
        }
        LabeledSlider {
            Kirigami.FormData.label: i18n("Volume / brightness, slide per step:")
            from: 2; to: 30; unit: i18n("units")
            value: kcm.stepDistance
            hint: i18n("Smaller means faster volume and brightness changes.")
            onEdited: newValue => kcm.stepDistance = newValue
        }
        LabeledSlider {
            Kirigami.FormData.label: i18n("Single actions, slide before running:")
            from: 10; to: 200; stepSize: 5; unit: i18n("units")
            value: kcm.triggerDistance
            hint: i18n("How far to swipe before a single action (like Show desktop) runs.")
            onEdited: newValue => kcm.triggerDistance = newValue
        }
        LabeledSlider {
            Kirigami.FormData.label: i18n("Finger-change grace time:")
            from: 100; to: 600; stepSize: 25; unit: i18n("ms")
            value: kcm.fingerChangeGrace
            hint: i18n("Keeps a 4-finger action going while the touchpad recounts fingers.")
            onEdited: newValue => kcm.fingerChangeGrace = Math.round(newValue)
        }

        Item {
            Kirigami.FormData.isSection: true
        }

        ColumnLayout {
            Kirigami.FormData.label: i18n("Finger detection:")
            spacing: 0
            QQC2.CheckBox {
                text: i18n("Ignore a resting thumb")
                checked: kcm.ignoreRestingThumb
                onToggled: kcm.ignoreRestingThumb = checked
            }
            QQC2.Label {
                Layout.maximumWidth: Kirigami.Units.gridUnit * 22
                text: i18n("Don't count a thumb resting at the bottom of the touchpad as a finger.")
                wrapMode: Text.WordWrap
                font: Kirigami.Theme.smallFont
                opacity: 0.75
            }
        }

        Item {
            Kirigami.FormData.isSection: true
        }

        QQC2.Label {
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
            text: i18n("These take effect when you press Apply.")
            wrapMode: Text.WordWrap
            opacity: 0.75
        }
    }
}
