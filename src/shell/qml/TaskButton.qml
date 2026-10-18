import QtQuick
import Noir.Shell
import Noir.Ui

// One running window in the taskbar. Active window = inverted (white) tile;
// minimized windows are dimmed; urgent windows pulse their underline.
NoirButton {
    id: task

    property string title
    property url windowIcon
    property bool active: false
    property bool minimized: false
    property bool attention: false

    checked: active
    flat: true
    toolTip: title
    width: 196
    height: Theme.taskbarHeight - 10

    Item {
        anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
        opacity: task.minimized && !task.inverted ? 0.55 : 1

        Image {
            id: appIcon
            anchors.verticalCenter: parent.verticalCenter
            width: 20; height: 20
            source: task.windowIcon
            sourceSize: Qt.size(40, 40)
            smooth: true
            // Third-party icons are drawn untouched; fall back to the Noir glyph.
            visible: status === Image.Ready && implicitWidth > 1
        }
        NoirIcon {
            anchors.verticalCenter: parent.verticalCenter
            visible: !appIcon.visible
            size: 20
            source: Theme.icon("app")
            color: task.foreground
        }
        Text {
            anchors { left: parent.left; leftMargin: 30; right: parent.right; verticalCenter: parent.verticalCenter }
            text: task.title
            elide: Text.ElideRight
            color: task.foreground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            font.letterSpacing: 0.4
        }
    }

    // Underline marks windows that are open; it pulses when one wants attention.
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 2
        color: task.inverted ? Theme.black : Theme.silver
        opacity: task.minimized ? 0.3 : 0.9
        SequentialAnimation on opacity {
            running: task.attention && !Theme.reducedMotion
            loops: Animation.Infinite
            NumberAnimation { to: 0.1; duration: 500 }
            NumberAnimation { to: 1.0; duration: 500 }
        }
    }
}
