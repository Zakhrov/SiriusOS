import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import Noir.Shell
import Noir.Ui

// Desktop widget shell: chamfered polygon of black glass that hovers above
// the wallpaper (slow bob + counter-moving soft shadow). Draggable. The glass
// is frosted: the wallpaper behind it shows through, blurred.
Item {
    id: widget

    property string title
    property int chamfer: Theme.chamfer
    property real floatAmplitude: 4
    property real phase: 0            // ms offset so widgets don't bob in sync
    property real bob: 0
    // What the glass shows through (the desktop's wallpaper), blurred.
    property Item backdrop
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

        // Frosted backdrop: the wallpaper under the widget, blurred and cut
        // to the widget's chamfered outline.
        ShaderEffectSource {
            id: behind
            visible: false
            sourceItem: widget.backdrop
            live: true
            sourceRect: Qt.rect(widget.x, widget.y - widget.lift, surface.width, surface.height)
        }
        Item {
            id: outline
            anchors.fill: parent
            visible: false
            layer.enabled: true
            Shape {
                anchors.fill: parent
                antialiasing: true
                ShapePath {
                    strokeWidth: -1
                    fillColor: "white"
                    startX: widget.chamfer; startY: 0
                    PathLine { x: surface.width; y: 0 }
                    PathLine { x: surface.width; y: surface.height - widget.chamfer }
                    PathLine { x: surface.width - widget.chamfer; y: surface.height }
                    PathLine { x: 0; y: surface.height }
                    PathLine { x: 0; y: widget.chamfer }
                    PathLine { x: widget.chamfer; y: 0 }
                }
            }
        }
        MultiEffect {
            anchors.fill: parent
            visible: widget.backdrop !== null && Theme.glassBlur
            source: behind
            autoPaddingEnabled: false
            blurEnabled: true
            blur: 1.0
            blurMax: 48
            saturation: -0.4
            maskEnabled: true
            maskSource: outline
        }

        Shape {
            anchors.fill: parent
            antialiasing: true
            ShapePath {
                strokeColor: widget.activeFocus ? Theme.white : Theme.glassEdge
                strokeWidth: widget.activeFocus ? 2 : 1
                fillGradient: LinearGradient {
                    x1: 0; y1: 0; x2: 0; y2: surface.height
                    GradientStop { position: 0; color: Theme.glassTop }
                    GradientStop { position: 1; color: Theme.glassBottom }
                }
                startX: widget.chamfer; startY: 0
                PathLine { x: surface.width; y: 0 }
                PathLine { x: surface.width; y: surface.height - widget.chamfer }
                PathLine { x: surface.width - widget.chamfer; y: surface.height }
                PathLine { x: 0; y: surface.height }
                PathLine { x: 0; y: widget.chamfer }
                PathLine { x: widget.chamfer; y: 0 }
            }
            // The light catching the glass, on the same outline.
            ShapePath {
                strokeWidth: -1
                fillGradient: LinearGradient {
                    x1: 0; y1: 0; x2: surface.width * 0.7; y2: surface.height
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.28; color: Theme.glassReflection }
                    GradientStop { position: 0.34; color: Qt.rgba(1, 1, 1, Theme.glassReflection.a * 0.3) }
                    GradientStop { position: 0.6; color: "transparent" }
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
