import QtQuick
import Noir.Ui

// Vertical scroll bar for a Flickable: a 3D groove cut into the surface with
// a bevelled silver handle that glides to its new position. The handle can be
// dragged, and a click in the groove pages towards it. Hidden when everything
// fits.
Item {
    id: bar

    property Flickable flickable
    readonly property real ratio: flickable ? flickable.visibleArea.heightRatio : 1

    width: 8
    visible: flickable && flickable.contentHeight > flickable.height + 1

    function scrollTo(position) {
        const range = flickable.contentHeight - flickable.height
        flickable.contentY = flickable.originY + Math.max(0, Math.min(range, position * flickable.contentHeight))
    }

    Rectangle { anchors.fill: parent; color: Theme.abyss; border.width: 1; border.color: Theme.silverFaint }
    Rectangle { anchors { left: parent.left; top: parent.top; bottom: parent.bottom } width: 1; color: Qt.rgba(0, 0, 0, 0.9) }

    Rectangle {
        id: handle
        x: 1
        width: parent.width - 2
        height: Math.max(24, bar.ratio * bar.height)
        y: bar.flickable ? Math.max(0, Math.min(bar.height - height, bar.flickable.visibleArea.yPosition * bar.height)) : 0
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: drag.containsMouse || drag.pressed ? Theme.white : Theme.silver }
            GradientStop { position: 1; color: Theme.silverDim }
        }
        Behavior on y { enabled: !drag.pressed; NumberAnimation { duration: Theme.durationFast; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        id: drag
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        property real grabOffset: -1
        onPressed: (event) => {
            if (event.y >= handle.y && event.y <= handle.y + handle.height) {
                grabOffset = event.y - handle.y
            } else {
                grabOffset = -1
                const page = bar.flickable.visibleArea.heightRatio
                bar.scrollTo(bar.flickable.visibleArea.yPosition + (event.y < handle.y ? -page : page))
            }
        }
        onPositionChanged: (event) => {
            if (pressed && grabOffset >= 0)
                bar.scrollTo((event.y - grabOffset) / bar.height)
        }
        onReleased: grabOffset = -1
    }
}
