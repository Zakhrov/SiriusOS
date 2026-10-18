import QtQuick
import Noir.Ui

// On/off switch: a square bevelled knob that slides along a recessed groove,
// which lights up silver when on. Space or Enter toggles it.
//
// `checked` is the state to show, normally bound to a setting; the switch
// never assigns it (that would break the binding). A click flips what it
// shows at once and emits toggled(); the owner then updates the setting.
FocusScope {
    id: control

    property bool checked: false
    property bool on: checked
    onCheckedChanged: on = checked
    property string accessibleName
    readonly property bool hovered: mouse.containsMouse
    readonly property bool highlighted: hovered || activeFocus

    // Emitted for user changes only, with the new state.
    signal toggled(bool checked)

    function toggle() {
        if (!enabled)
            return
        on = !on
        toggled(on)
    }

    implicitWidth: 48
    implicitHeight: 24
    activeFocusOnTab: true
    opacity: enabled ? 1 : 0.4

    Accessible.role: Accessible.CheckBox
    Accessible.name: accessibleName
    Accessible.checkable: true
    Accessible.checked: on
    Accessible.onToggleAction: toggle()
    Accessible.onPressAction: toggle()

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            toggle()
            event.accepted = true
        }
    }

    // Groove, cut into the surface.
    Rectangle {
        id: groove
        anchors.fill: parent
        color: control.on ? Theme.silver : Theme.abyss
        border.width: Theme.edge
        border.color: control.highlighted ? Theme.white : Theme.silverFaint
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 } height: 1; color: Qt.rgba(0, 0, 0, 0.8) }
    }

    Text {
        anchors { verticalCenter: parent.verticalCenter; left: parent.left; leftMargin: 7 }
        visible: control.on
        text: "I"
        color: Theme.black
        font.pixelSize: Theme.fontSmall
        font.bold: true
    }

    // Knob: raised, lit from the top-left, sinks while pressed.
    Rectangle {
        id: knob
        y: 3
        width: parent.height - 6
        height: width
        x: control.on ? parent.width - width - 3 : 3
        color: control.highlighted ? Theme.white : (control.on ? Theme.black : Theme.silverDim)
        border.width: Theme.edge
        border.color: control.on ? Theme.black : Theme.silver
        scale: mouse.pressed ? Theme.pressDepth : 1
        Behavior on x { NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutBack; easing.overshoot: 1.4 } }
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
        Behavior on scale { NumberAnimation { duration: Theme.durationFast } }
        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 } height: 1; color: Qt.rgba(1, 1, 1, 0.5) }
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 1 } height: 1; color: Qt.rgba(0, 0, 0, 0.6) }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            control.forceActiveFocus(Qt.MouseFocusReason)
            control.toggle()
        }
    }
}
