// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// The action for one direction, plus its command or KDE action when it needs one.
RowLayout {
    id: dir

    required property string label
    required property string action
    required property string target
    required property var singles
    signal actionPicked(string value)
    signal targetEdited(string value)

    readonly property string kdeLabel: action === "kde-action" && target !== "" ? kcm.kdeActionLabel(target) : ""

    function indexIn(list, value) {
        for (let i = 0; i < list.length; ++i) {
            if (list[i].value === value) {
                return i;
            }
        }
        return -1;
    }

    spacing: Kirigami.Units.smallSpacing

    QQC2.Label {
        Layout.preferredWidth: Kirigami.Units.gridUnit * 3
        text: dir.label
    }
    QQC2.ComboBox {
        Layout.fillWidth: true
        Layout.minimumWidth: Kirigami.Units.gridUnit * 7
        Layout.maximumWidth: Kirigami.Units.gridUnit * 14
        model: dir.singles
        textRole: "text"
        valueRole: "value"
        currentIndex: dir.indexIn(dir.singles, dir.action)
        onActivated: dir.actionPicked(currentValue)
        Accessible.name: i18nc("@label direction, e.g. Up", "%1: action", dir.label)
    }
    QQC2.TextField {
        visible: dir.action === "command"
        Layout.fillWidth: true
        Layout.minimumWidth: Kirigami.Units.gridUnit * 6
        Layout.maximumWidth: Kirigami.Units.gridUnit * 16
        text: dir.target
        placeholderText: i18n("Command, e.g. spectacle -r")
        onTextEdited: dir.targetEdited(text)
        Accessible.name: i18nc("@label direction, e.g. Up", "%1: command to run", dir.label)
    }
    QQC2.Button {
        visible: dir.action === "kde-action"
        Layout.minimumWidth: Kirigami.Units.gridUnit * 6
        Layout.maximumWidth: Kirigami.Units.gridUnit * 16
        text: dir.target === "" ? i18n("Choose Action…") : (dir.kdeLabel !== "" ? dir.kdeLabel : dir.target)
        icon.name: dir.target !== "" && dir.kdeLabel === "" ? "dialog-warning" : "configure-shortcuts"
        onClicked: picker.open()
    }
    QQC2.Label {
        visible: dir.action === "kde-action" && dir.target !== "" && dir.kdeLabel === ""
        text: i18n("Action not found")
        color: Kirigami.Theme.negativeTextColor
    }

    KdeActionDialog {
        id: picker
        onPicked: target => dir.targetEdited(target)
    }
}
