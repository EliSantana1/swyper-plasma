// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Pick one of KDE's global actions (everything that can have a global shortcut).
Kirigami.Dialog {
    id: dialog

    signal picked(string target)
    property var allActions: []
    readonly property var shown: {
        const q = search.text.trim().toLowerCase();
        return q === "" ? allActions : allActions.filter(a => a.label.toLowerCase().includes(q));
    }

    parent: QQC2.Overlay.overlay // centred over the window, not squeezed into the row it is declared in
    title: i18n("Choose a KDE action")
    preferredWidth: Kirigami.Units.gridUnit * 30
    preferredHeight: Kirigami.Units.gridUnit * 28
    standardButtons: Kirigami.Dialog.Cancel
    padding: Kirigami.Units.largeSpacing

    onOpened: {
        if (allActions.length === 0) {
            allActions = kcm.kdeActions();
        }
        search.text = "";
        search.forceActiveFocus();
    }

    ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.SearchField {
            id: search
            Layout.fillWidth: true
        }
        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ListView {
                id: list
                clip: true
                model: dialog.shown
                delegate: QQC2.ItemDelegate {
                    required property var modelData
                    width: ListView.view.width
                    text: modelData.label
                    onClicked: {
                        dialog.picked(modelData.target);
                        dialog.close();
                    }
                }
                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 4
                    visible: list.count === 0
                    text: dialog.allActions.length === 0 ? i18n("Could not read KDE's actions") : i18n("No matching actions")
                }
            }
        }
    }
}
