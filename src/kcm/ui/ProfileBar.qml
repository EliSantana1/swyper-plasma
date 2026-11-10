// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Saved profiles, plus import / export of .swyper files for sharing.
GridLayout {
    id: bar

    signal message(string text, int type) // shown by the page as an inline message
    required property real availableWidth // the page's width; decides the layout without depending on our own size

    // the two groups side by side when there is room, stacked when the window is narrow
    columns: availableWidth >= profileGroup.implicitWidth + fileGroup.implicitWidth + Kirigami.Units.gridUnit * 2 ? 2 : 1
    columnSpacing: Kirigami.Units.smallSpacing
    rowSpacing: Kirigami.Units.smallSpacing

    RowLayout {
    id: profileGroup
    Layout.fillWidth: true // takes the spare room, so the file buttons sit at the right edge
    spacing: Kirigami.Units.smallSpacing
    QQC2.Label {
        text: i18n("Profile:")
    }
    QQC2.ComboBox {
        Layout.preferredWidth: Kirigami.Units.gridUnit * 14
        Layout.minimumWidth: Kirigami.Units.gridUnit * 8
        model: kcm.profiles
        enabled: kcm.profiles.length > 0
        currentIndex: kcm.profiles.indexOf(kcm.activeProfile)
        displayText: kcm.activeProfile !== "" ? kcm.activeProfile
                   : kcm.profiles.length === 0 ? i18n("No saved profiles") : i18n("Custom (not saved)")
        onActivated: index => kcm.loadProfile(kcm.profiles[index])
        Accessible.name: i18n("Profile")
    }
    QQC2.Button {
        text: i18n("Save As…")
        icon.name: "document-save-as"
        onClicked: saveDialog.open()
    }
    QQC2.Button {
        text: i18n("Delete")
        icon.name: "edit-delete"
        enabled: kcm.activeProfile !== ""
        onClicked: {
            deleteDialog.profile = kcm.activeProfile;
            deleteDialog.open();
        }
    }
    Item {
        Layout.fillWidth: true // without a stretchable item the group (and the whole bar) could not grow past its natural width
    }
    }

    RowLayout {
    id: fileGroup
    Layout.alignment: bar.columns === 2 ? Qt.AlignRight : Qt.AlignLeft
    spacing: Kirigami.Units.smallSpacing
    QQC2.Button {
        text: i18n("Import…")
        icon.name: "document-import"
        onClicked: importPicker.open()
    }
    QQC2.Button {
        text: i18n("Export…")
        icon.name: "document-export"
        onClicked: exportPicker.open()
    }
    }

    // Dialogs live on the window's overlay; inside this row they would be squeezed into the row's size.
    Kirigami.PromptDialog {
        id: saveDialog
        parent: QQC2.Overlay.overlay
        preferredWidth: Kirigami.Units.gridUnit * 22
        title: i18n("Save profile")
        standardButtons: Kirigami.Dialog.Save | Kirigami.Dialog.Cancel
        onOpened: {
            saveError.text = "";
            nameField.text = kcm.activeProfile;
            nameField.forceActiveFocus();
        }
        onAccepted: {
            const name = nameField.text.trim();
            const error = kcm.saveProfile(name);
            if (error !== "") {
                saveError.text = error;
                open();
            } else {
                bar.message(i18n("Saved as profile “%1”.", name), Kirigami.MessageType.Positive);
            }
        }
        ColumnLayout {
            QQC2.TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: i18n("Profile name")
                onAccepted: saveDialog.accept()
            }
            QQC2.Label {
                Layout.fillWidth: true
                visible: nameField.text.trim() !== "" && kcm.profileExists(nameField.text)
                text: i18n("A profile with this name already exists and will be replaced.")
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.neutralTextColor
            }
            QQC2.Label {
                id: saveError
                Layout.fillWidth: true
                visible: text !== ""
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.negativeTextColor
            }
        }
    }

    Kirigami.PromptDialog {
        id: deleteDialog
        parent: QQC2.Overlay.overlay
        property string profile
        title: i18n("Delete profile")
        subtitle: i18n("Delete the profile “%1”? This cannot be undone. Your current gestures stay as they are.", profile)
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Delete")
                icon.name: "edit-delete"
                onTriggered: {
                    kcm.deleteProfile(deleteDialog.profile);
                    deleteDialog.close();
                }
            }
        ]
    }

    Dialogs.FileDialog {
        id: importPicker
        title: i18n("Import a gestures profile")
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: [i18n("Swyper profiles (*.swyper *.gestures)"), i18n("All files (*)")]
        onAccepted: {
            importDialog.url = selectedFile;
            importDialog.info = kcm.inspectFile(selectedFile);
            importDialog.open();
        }
    }

    Dialogs.FileDialog {
        id: exportPicker
        title: i18n("Export these gestures")
        fileMode: Dialogs.FileDialog.SaveFile
        defaultSuffix: "swyper"
        nameFilters: [i18n("Swyper profiles (*.swyper)")]
        onAccepted: {
            const error = kcm.exportFile(selectedFile);
            if (error !== "") {
                bar.message(error, Kirigami.MessageType.Error);
            } else {
                bar.message(i18n("Exported. Others can open the file with Import."), Kirigami.MessageType.Positive);
            }
        }
    }

    // Shows what an imported file would do, including every command it runs, before it is used.
    Kirigami.Dialog {
        id: importDialog
        parent: QQC2.Overlay.overlay
        property url url
        property var info: ({})
        readonly property bool ok: info.ok === true
        readonly property var commands: ok ? info.commands : []
        readonly property var problems: ok ? info.problems : []

        title: ok ? i18n("Import “%1”", info.name) : i18n("Can't import this file")
        preferredWidth: Kirigami.Units.gridUnit * 26
        padding: Kirigami.Units.largeSpacing
        standardButtons: ok ? Kirigami.Dialog.Cancel : Kirigami.Dialog.Close
        customFooterActions: [
            Kirigami.Action {
                visible: importDialog.ok
                text: i18n("Use This Profile")
                icon.name: "dialog-ok-apply"
                onTriggered: {
                    const error = kcm.importFile(importDialog.url, keepBox.checked);
                    importDialog.close();
                    if (error !== "") {
                        bar.message(error, Kirigami.MessageType.Error);
                    } else {
                        bar.message(i18n("Profile loaded. Press Apply to start using it."), Kirigami.MessageType.Information);
                    }
                }
            }
        ]

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                Layout.fillWidth: true
                visible: !importDialog.ok
                text: importDialog.info.error ?? ""
                wrapMode: Text.WordWrap
            }
            QQC2.Label {
                Layout.fillWidth: true
                visible: importDialog.ok
                text: i18n("The profile is loaded into this page; nothing changes until you press Apply.")
                wrapMode: Text.WordWrap
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: importDialog.commands.length > 0
                type: Kirigami.MessageType.Warning
                text: i18np("This profile runs a command when you swipe. Only use it if you trust where it came from:",
                            "This profile runs commands when you swipe. Only use it if you trust where it came from:",
                            importDialog.commands.length)
            }
            Repeater {
                model: importDialog.commands
                delegate: QQC2.Label {
                    required property string modelData
                    Layout.fillWidth: true
                    text: modelData
                    font.family: "monospace"
                    wrapMode: Text.WrapAnywhere
                }
            }
            Repeater {
                model: importDialog.problems
                delegate: QQC2.Label {
                    required property string modelData
                    Layout.fillWidth: true
                    text: "• " + modelData
                    wrapMode: Text.WordWrap
                    color: Kirigami.Theme.neutralTextColor
                }
            }
            QQC2.CheckBox {
                id: keepBox
                visible: importDialog.ok
                checked: true
                text: i18n("Also keep it in my profiles")
            }
        }
    }
}
