// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// One axis of one finger count: its action, a hint, and the per-direction actions for "separate".
ColumnLayout {
    id: row

    required property string title
    required property var axis
    required property var actions
    required property var singles
    required property string firstLabel
    required property string secondLabel

    readonly property int labelWidth: Kirigami.Units.gridUnit * 9
    readonly property var current: {
        const i = indexIn(actions, axis.action);
        return i >= 0 ? actions[i] : null;
    }

    function indexIn(list, value) {
        for (let i = 0; i < list.length; ++i) {
            if (list[i].value === value) {
                return i;
            }
        }
        return -1;
    }

    spacing: Kirigami.Units.smallSpacing

    RowLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            Layout.preferredWidth: row.labelWidth
            text: row.title
            font.bold: true
        }
        QQC2.ComboBox {
            Layout.fillWidth: true
            Layout.minimumWidth: Kirigami.Units.gridUnit * 8 // lets the row shrink in a narrow window
            Layout.preferredWidth: Kirigami.Units.gridUnit * 22
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
            model: row.actions
            textRole: "text"
            valueRole: "value"
            currentIndex: row.indexIn(row.actions, row.axis.action)
            onActivated: row.axis.action = currentValue
            Accessible.name: row.title
        }
        RowLayout {
            visible: row.current !== null && row.current.follows === true
            spacing: Kirigami.Units.smallSpacing
            Kirigami.Icon {
                source: "input-touchpad"
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
            }
            QQC2.Label {
                text: i18n("Follows your fingers")
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.positiveTextColor
            }
        }
        Item { Layout.fillWidth: true }
    }

    QQC2.Label {
        Layout.fillWidth: true
        Layout.leftMargin: row.labelWidth + Kirigami.Units.largeSpacing
        text: row.current ? row.current.hint : ""
        wrapMode: Text.WordWrap
        font: Kirigami.Theme.smallFont
        opacity: 0.75
    }

    DirectionRow {
        visible: row.axis.action === "separate"
        Layout.fillWidth: true
        Layout.leftMargin: row.labelWidth + Kirigami.Units.largeSpacing
        label: row.firstLabel
        action: row.axis.firstAction
        target: row.axis.firstTarget
        singles: row.singles
        onActionPicked: value => {
            row.axis.firstTarget = "";
            row.axis.firstAction = value;
        }
        onTargetEdited: value => row.axis.firstTarget = value
    }
    DirectionRow {
        visible: row.axis.action === "separate"
        Layout.fillWidth: true
        Layout.leftMargin: row.labelWidth + Kirigami.Units.largeSpacing
        label: row.secondLabel
        action: row.axis.secondAction
        target: row.axis.secondTarget
        singles: row.singles
        onActionPicked: value => {
            row.axis.secondTarget = "";
            row.axis.secondAction = value;
        }
        onTargetEdited: value => row.axis.secondTarget = value
    }
}
