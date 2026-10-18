import QtQuick
import Noir.Ui

// Base interactive element. Inverts colours on hover/keyboard focus and sinks
// with a 3D press (scale + bevel flip) when activated by mouse or keyboard.
FocusScope {
    id: button

    property string text
    property url iconSource
    property string toolTip
    property bool checked: false
    property bool flat: false          // no idle border (taskbar items)
    property bool showText: text.length > 0
    property int iconSize: Theme.iconSize
    property int horizontalPadding: 10
    property alias hovered: mouse.containsMouse
    readonly property bool pressed: mouse.pressed || keyPressed
    readonly property bool inverted: hovered || activeFocus || checked
    readonly property color foreground: inverted ? Theme.invertedForeground : Theme.silver
    readonly property color background: inverted ? Theme.invertedBackground
                                                 : (flat ? "transparent" : Theme.glass)
    property bool keyPressed: false

    default property alias extraContent: extra.data

    signal clicked()
    signal rightClicked()
    signal middleClicked()

    implicitWidth: row.implicitWidth + horizontalPadding * 2
    implicitHeight: Math.max(iconSize, label.implicitHeight) + 16
    activeFocusOnTab: true
    // Disabled buttons fade back; they still show their state.
    opacity: enabled ? 1 : 0.35
    Behavior on opacity { NumberAnimation { duration: Theme.durationFast } }

    Accessible.role: Accessible.Button
    Accessible.name: text.length ? text : toolTip
    Accessible.onPressAction: clicked()

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            keyPressed = true
            event.accepted = true
        }
    }
    Keys.onReleased: (event) => {
        if (keyPressed && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space)) {
            keyPressed = false
            event.accepted = true
            button.clicked()
        }
    }

    Item {
        id: face
        anchors.fill: parent
        // 3D press: the face sinks slightly into the surface.
        scale: button.pressed ? Theme.pressDepth : 1.0
        transform: Translate { y: button.pressed ? 1 : 0 }
        Behavior on scale { NumberAnimation { duration: Theme.durationFast; easing.type: Easing.OutQuad } }

        Rectangle {
            anchors.fill: parent
            radius: Theme.radius
            color: button.background
            border.width: button.flat && !button.inverted ? 0 : Theme.edge
            border.color: button.inverted ? Theme.white : Theme.glassEdge
            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
        }

        // Bevel: light from the top-left when raised, flipped when pressed.
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 1
            color: button.pressed ? Theme.abyss : Qt.rgba(1, 1, 1, button.inverted ? 0.9 : 0.18)
            visible: !button.flat || button.inverted
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: button.pressed ? Qt.rgba(1, 1, 1, 0.35) : Qt.rgba(0, 0, 0, 0.6)
            visible: !button.flat || button.inverted
        }

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8
            NoirIcon {
                visible: button.iconSource.toString().length > 0
                anchors.verticalCenter: parent.verticalCenter
                source: button.iconSource
                size: button.iconSize
                color: button.foreground
            }
            Text {
                id: label
                visible: button.showText
                anchors.verticalCenter: parent.verticalCenter
                text: button.text
                color: button.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                font.letterSpacing: Theme.letterSpacing
                elide: Text.ElideRight
            }
        }

        Item {
            id: extra
            anchors.fill: parent
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onClicked: (mouseEvent) => {
            if (mouseEvent.button === Qt.RightButton)
                button.rightClicked()
            else if (mouseEvent.button === Qt.MiddleButton)
                button.middleClicked()
            else
                button.clicked()
        }
    }

    NoirToolTip {
        target: button
        text: button.toolTip
        shown: button.hovered && button.toolTip.length > 0
    }
}
