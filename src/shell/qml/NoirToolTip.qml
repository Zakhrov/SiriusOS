import QtQuick
import QtQuick.Window
import Noir.Shell

// Tooltip hosted in its own override-redirect window so it can extend beyond
// the (thin) taskbar surface. Pops up with a 3D tilt and a slight bounce.
Item {
    id: tip

    property Item target
    property string text
    property bool shown: false
    property int delay: 550

    Timer {
        id: showTimer
        interval: tip.delay
        onTriggered: popup.open()
    }

    onShownChanged: {
        if (shown && text.length > 0) {
            showTimer.restart()
        } else {
            showTimer.stop()
            popup.close()
        }
    }

    Window {
        id: popup

        property bool opened: false

        function open() {
            if (!tip.target || !tip.target.Window.window)
                return
            const w = label.implicitWidth + 20
            const h = label.implicitHeight + 12
            const p = tip.target.mapToGlobal(tip.target.width / 2, 0)
            width = w
            height = h
            const screenRight = tip.target.Window.window.screen.virtualX + tip.target.Window.window.screen.width
            x = Math.max(0, Math.min(Math.round(p.x - w / 2), screenRight - w))
            // Prefer above the target; fall back to below it near the top edge.
            y = p.y - h - 6 >= 0 ? Math.round(p.y - h - 6)
                                 : Math.round(tip.target.mapToGlobal(0, tip.target.height).y + 6)
            visible = true
            opened = true
        }
        function close() {
            opened = false
            visible = false
        }

        flags: Qt.ToolTip | Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
        color: Theme.black
        visible: false

        Item {
            id: bubble
            anchors.fill: parent
            transformOrigin: Item.Bottom
            scale: popup.opened ? 1.0 : 0.6
            opacity: popup.opened ? 1.0 : 0.0
            transform: Rotation {
                origin.x: bubble.width / 2
                origin.y: bubble.height
                axis { x: 1; y: 0; z: 0 }
                angle: popup.opened ? 0 : 50
                Behavior on angle { NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutBack } }
            }
            Behavior on scale { NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutBack; easing.overshoot: 2.2 } }
            Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }

            GlassPanel {
                anchors.fill: parent
                color: Theme.glassStrong
            }
            Text {
                id: label
                anchors.centerIn: parent
                text: tip.text
                color: Theme.white
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                font.letterSpacing: Theme.letterSpacing
            }
        }
    }
}
