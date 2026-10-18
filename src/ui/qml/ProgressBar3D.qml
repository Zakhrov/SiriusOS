import QtQuick
import Noir.Shell

// Recessed groove with a bevelled silver fill that glides to its new value,
// plus an optional percentage readout.
Item {
    id: bar

    property real value: 0          // 0..1
    property bool showPercentage: true
    property real animatedValue: value
    Behavior on animatedValue { NumberAnimation { duration: Theme.durationSlow; easing.type: Easing.OutCubic } }

    implicitHeight: 12
    implicitWidth: 160

    Accessible.role: Accessible.ProgressBar
    Accessible.name: Math.round(bar.value * 100) + "%"

    Item {
        id: groove
        anchors { left: parent.left; right: percent.visible ? percent.left : parent.right
                  rightMargin: percent.visible ? 8 : 0; verticalCenter: parent.verticalCenter }
        height: bar.height

        // Groove is cut into the surface: dark top edge, light bottom edge.
        Rectangle { anchors.fill: parent; color: Theme.abyss; border.width: 1; border.color: Theme.silverFaint }
        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 1; color: Qt.rgba(0, 0, 0, 0.9) }
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Qt.rgba(1, 1, 1, 0.12) }

        Rectangle {
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 2 }
            width: Math.max(0, (groove.width - 4) * Math.min(1, Math.max(0, bar.animatedValue)))
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.white }
                GradientStop { position: 0.45; color: Theme.silver }
                GradientStop { position: 1.0; color: Theme.silverDim }
            }
        }
    }

    Text {
        id: percent
        visible: bar.showPercentage
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        width: 34
        horizontalAlignment: Text.AlignRight
        text: Math.round(bar.animatedValue * 100) + "%"
        color: Theme.silver
        font.family: Theme.monoFamily
        font.pixelSize: Theme.fontSmall
    }
}
