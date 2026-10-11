import QtQuick
import QtQuick.Shapes
import Noir.Shell

// Desktop widget shell: chamfered polygon of black glass that hovers above
// the wallpaper (slow bob + counter-moving soft shadow). Draggable.
Item {
    id: widget

    property string title
    property int chamfer: Theme.chamfer
    property real floatAmplitude: 4
    property real phase: 0            // ms offset so widgets don't bob in sync
    property real bob: 0
    default property alias content: body.data

    activeFocusOnTab: true
    Accessible.role: Accessible.Grouping
    Accessible.name: title

    SequentialAnimation on bob {
        running: !Theme.reducedMotion
        loops: Animation.Infinite
        PauseAnimation { duration: widget.phase }
        NumberAnimation { from: 0; to: 1; duration: Theme.floatPeriod / 2; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1; to: 0; duration: Theme.floatPeriod / 2; easing.type: Easing.InOutSine }
    }

    readonly property real lift: bob * floatAmplitude

    // Soft drop shadow: stacked, slightly larger polygons. It spreads and
    // fades as the widget rises.
    Repeater {
        model: 5
        Shape {
            readonly property real grow: (index + 1) * 2.5 + widget.lift
            x: 6 + widget.lift * 0.5 - grow / 2
            y: 12 + widget.lift - grow / 2
            width: widget.width + grow
            height: widget.height + grow
            opacity: 0.11 - widget.lift * 0.008
            ShapePath {
                strokeWidth: -1
                fillColor: Theme.abyss
                startX: widget.chamfer; startY: 0
                PathLine { x: width; y: 0 }
                PathLine { x: width; y: height - widget.chamfer }
                PathLine { x: width - widget.chamfer; y: height }
                PathLine { x: 0; y: height }
                PathLine { x: 0; y: widget.chamfer }
                PathLine { x: widget.chamfer; y: 0 }
            }
        }
    }

    Item {
        id: surface
        width: widget.width
        height: widget.height
        y: -widget.lift

        Shape {
            anchors.fill: parent
            antialiasing: true
            ShapePath {
                strokeColor: widget.activeFocus ? Theme.white : Theme.glassEdge
                strokeWidth: widget.activeFocus ? 2 : 1
                fillGradient: LinearGradient {
                    x1: 0; y1: 0; x2: 0; y2: surface.height
                    GradientStop { position: 0; color: Qt.rgba(0.09, 0.095, 0.11, 0.82) }
                    GradientStop { position: 1; color: Qt.rgba(0.01, 0.012, 0.016, 0.86) }
                }
                startX: widget.chamfer; startY: 0
                PathLine { x: surface.width; y: 0 }
                PathLine { x: surface.width; y: surface.height - widget.chamfer }
                PathLine { x: surface.width - widget.chamfer; y: surface.height }
                PathLine { x: 0; y: surface.height }
                PathLine { x: 0; y: widget.chamfer }
                PathLine { x: widget.chamfer; y: 0 }
            }
        }

        // Title strip with a silver tick, like a HUD readout.
        Row {
            id: header
            visible: widget.title.length > 0
            x: widget.chamfer + 6
            y: 10
            spacing: 8
            Rectangle { width: 3; height: 12; color: Theme.white; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: widget.title.toUpperCase()
                color: Theme.silverDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                font.letterSpacing: 2.5
            }
        }

        Item {
            id: body
            anchors { fill: parent; margins: 16; topMargin: header.visible ? 32 : 16 }
        }
    }

    MouseArea {
        anchors.fill: parent
        drag.target: widget
        drag.threshold: 6
        cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.ArrowCursor
        onPressed: widget.forceActiveFocus()
    }
}
