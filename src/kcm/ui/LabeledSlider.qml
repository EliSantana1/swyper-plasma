// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A slider with its value and a short hint underneath, for a Kirigami.FormLayout row.
ColumnLayout {
    id: slider

    property real from
    property real to
    property real stepSize: 1
    property real value
    property string unit
    property string hint
    signal edited(real newValue)

    spacing: 0

    RowLayout {
        QQC2.Slider {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 14
            from: slider.from
            to: slider.to
            // no stepSize here (it draws a dense row of tick marks); values are snapped to steps instead
            value: slider.value
            onMoved: slider.edited(Math.round(value / slider.stepSize) * slider.stepSize)
            Accessible.name: slider.Kirigami.FormData.label
        }
        QQC2.Label {
            Layout.minimumWidth: Kirigami.Units.gridUnit * 4
            text: Math.round(slider.value) + " " + slider.unit
        }
    }
    QQC2.Label {
        Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        text: slider.hint
        wrapMode: Text.WordWrap
        font: Kirigami.Theme.smallFont
        opacity: 0.75
    }
}
