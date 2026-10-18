import QtQuick
import Noir.Shell
import Noir.Ui

// Desktop launcher: a bevelled black tile with a silver SVG glyph that
// inverts when selected and sinks when opened.
FocusScope {
    id: icon

    property string label
    property url iconSource
    property bool selected: false
    property bool keyPressed: false
    readonly property bool inverted: mouse.containsMouse || activeFocus || selected
    readonly property bool pressed: mouse.pressed || keyPressed

    signal activated()

    width: 92
    height: 100
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: activated()

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
            icon.activated()
        }
    }

    Item {
        id: tile
        width: Theme.desktopIconSize
        height: Theme.desktopIconSize
        anchors.horizontalCenter: parent.horizontalCenter
        y: 4
        scale: icon.pressed ? Theme.pressDepth : 1.0
        Behavior on scale { NumberAnimation { duration: Theme.durationFast } }

        // Extruded block: a dark side face offset down-right gives depth.
        Rectangle { x: 4; y: 4; width: parent.width; height: parent.height; color: Theme.abyss; opacity: 0.8 }
        Rectangle { x: 2; y: 2; width: parent.width; height: parent.height; color: Theme.silverFaint }
        Rectangle {
            anchors.fill: parent
            color: icon.inverted ? Theme.white : Theme.black
            border.width: 1
            border.color: icon.inverted ? Theme.white : Theme.silver
            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
        }
        Rectangle { x: 1; y: 1; width: parent.width - 2; height: 1; color: Qt.rgba(1, 1, 1, icon.inverted ? 1 : 0.3) }
        Rectangle { x: 1; y: 1; width: 1; height: parent.height - 2; color: Qt.rgba(1, 1, 1, icon.inverted ? 1 : 0.18) }

        NoirIcon {
            anchors.centerIn: parent
            size: 36
            source: icon.iconSource
            color: icon.inverted ? Theme.black : Theme.silver
        }
    }

    Rectangle {
        anchors { horizontalCenter: parent.horizontalCenter; top: tile.bottom; topMargin: 8 }
        width: Math.min(icon.width, caption.implicitWidth + 10)
        height: caption.implicitHeight + 4
        color: icon.inverted ? Theme.white : Qt.rgba(0, 0, 0, 0.5)
        Text {
            id: caption
            anchors.centerIn: parent
            width: Math.min(implicitWidth, icon.width - 10)
            text: icon.label
            color: icon.inverted ? Theme.black : Theme.white
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            font.letterSpacing: Theme.letterSpacing
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onPressed: icon.forceActiveFocus()
        onDoubleClicked: icon.activated()
    }
}
