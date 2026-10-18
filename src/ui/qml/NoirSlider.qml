import QtQuick
import Noir.Ui

// Horizontal slider: a recessed groove with a silver fill and a square
// bevelled handle. Arrow keys step, Page Up/Down take ten steps, Home/End jump
// to the ends.
//
// `value` is the value to show, normally bound to a setting; the slider never
// assigns it. While the user drags, `current` follows the handle and moved()
// reports each new value; the owner then updates the setting.
FocusScope {
    id: control

    property real from: 0
    property real to: 1
    property real stepSize: 0.01
    property real value: 0
    property real current: value
    property string accessibleName
    readonly property bool pressed: mouse.pressed
    readonly property real position: to > from ? (current - from) / (to - from) : 0
    readonly property bool highlighted: mouse.containsMouse || activeFocus || mouse.pressed

    // Emitted for user changes only, with the new value.
    signal moved(real value)

    onValueChanged: if (!mouse.pressed) current = value

    function setValue(v) {
        let stepped = stepSize > 0 ? from + Math.round((v - from) / stepSize) * stepSize : v
        stepped = Math.max(Math.min(from, to), Math.min(Math.max(from, to), stepped))
        if (Math.abs(stepped - current) < 1e-9)
            return
        current = stepped
        moved(stepped)
    }

    implicitWidth: 220
    implicitHeight: 24
    activeFocusOnTab: true
    opacity: enabled ? 1 : 0.4

    Accessible.role: Accessible.Slider
    Accessible.name: accessibleName
    Accessible.onIncreaseAction: setValue(current + stepSize)
    Accessible.onDecreaseAction: setValue(current - stepSize)

    Keys.onPressed: (event) => {
        const step = stepSize > 0 ? stepSize : (to - from) / 100
        switch (event.key) {
        case Qt.Key_Right: case Qt.Key_Up: setValue(current + step); break
        case Qt.Key_Left: case Qt.Key_Down: setValue(current - step); break
        case Qt.Key_PageUp: setValue(current + step * 10); break
        case Qt.Key_PageDown: setValue(current - step * 10); break
        case Qt.Key_Home: setValue(from); break
        case Qt.Key_End: setValue(to); break
        default: return
        }
        event.accepted = true
    }

    Item {
        id: track
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                  leftMargin: handle.width / 2; rightMargin: handle.width / 2 }
        height: 8

        Rectangle { anchors.fill: parent; color: Theme.abyss; border.width: 1; border.color: control.highlighted ? Theme.silverDim : Theme.silverFaint }
        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 1; color: Qt.rgba(0, 0, 0, 0.9) }
        Rectangle {
            x: 1; y: 1
            height: parent.height - 2
            width: Math.max(0, (parent.width - 2) * control.position)
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.white }
                GradientStop { position: 1; color: Theme.silverDim }
            }
        }
    }

    Rectangle {
        id: handle
        width: 14
        height: parent.height
        x: track.x + track.width * control.position - width / 2
        color: control.highlighted ? Theme.white : Theme.panel
        border.width: Theme.edge
        border.color: control.highlighted ? Theme.white : Theme.silver
        scale: mouse.pressed ? Theme.pressDepth : 1
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
        Behavior on scale { NumberAnimation { duration: Theme.durationFast } }
        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 } height: 1; color: Qt.rgba(1, 1, 1, 0.45) }
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 1 } height: 1; color: Qt.rgba(0, 0, 0, 0.6) }
        Rectangle { anchors.centerIn: parent; width: 2; height: parent.height / 2; color: control.highlighted ? Theme.black : Theme.silverDim }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        function follow(px) {
            control.setValue(control.from + Math.max(0, Math.min(1, (px - track.x) / track.width)) * (control.to - control.from))
        }
        onPressed: (event) => { control.forceActiveFocus(Qt.MouseFocusReason); follow(event.x) }
        onPositionChanged: (event) => { if (pressed) follow(event.x) }
        onWheel: (wheel) => {
            const step = control.stepSize > 0 ? control.stepSize : (control.to - control.from) / 100
            control.setValue(control.current + (wheel.angleDelta.y > 0 ? step : -step))
        }
    }
}
