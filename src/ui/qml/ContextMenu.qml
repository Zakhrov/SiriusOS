import QtQuick
import Noir.Ui

// Glass context menu that drops down with a 3D fold (hinged on its top edge)
// and staggers its items in. Fully keyboard navigable.
FocusScope {
    id: menu

    // [{ text: "...", icon: "icons/x.svg", action: function() {} } | { separator: true }]
    property var items: []
    property bool opened: false
    property Item restoreFocus

    signal triggered(int index)

    width: 220
    height: column.implicitHeight + 8
    visible: opened || fold.angle > -89
    z: 1000

    function popup(px, py) {
        restoreFocus = Window.activeFocusItem
        x = Math.min(px, parent.width - width - 4)
        y = Math.min(py, parent.height - height - 4)
        opened = true
        list.currentIndex = firstSelectable(0, 1)
        menu.forceActiveFocus()
    }
    function close() {
        opened = false
        if (restoreFocus)
            restoreFocus.forceActiveFocus()
    }
    function firstSelectable(from, step) {
        for (let i = from; i >= 0 && i < items.length; i += step) {
            if (!items[i].separator)
                return i
        }
        return list.currentIndex
    }
    function activate(index) {
        const item = items[index]
        close()
        if (item && item.action)
            item.action()
        triggered(index)
    }

    transform: Rotation {
        id: fold
        origin.x: menu.width / 2
        origin.y: 0
        axis { x: 1; y: 0; z: 0 }
        angle: menu.opened ? 0 : -90
        Behavior on angle { NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic } }
    }
    opacity: menu.opened ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Theme.durationNormal } }

    GlassPanel {
        anchors.fill: parent
        color: Theme.glassStrong
        elevation: 10
    }

    Column {
        id: column
        x: 4; y: 4
        width: parent.width - 8

        Repeater {
            id: list
            property int currentIndex: 0
            model: menu.items

            Item {
                id: entry
                required property var modelData
                required property int index
                readonly property bool current: list.currentIndex === index && !modelData.separator
                width: column.width
                height: modelData.separator ? 9 : 30
                // Stagger: each row slides in a little after the one above it.
                opacity: menu.opened ? 1 : 0
                transform: Translate { y: menu.opened ? 0 : -8 }
                Behavior on opacity {
                    SequentialAnimation {
                        PauseAnimation { duration: Theme.reducedMotion ? 0 : entry.index * 25 }
                        NumberAnimation { duration: Theme.durationNormal }
                    }
                }

                Rectangle {
                    visible: entry.modelData.separator === true
                    anchors.centerIn: parent
                    width: parent.width - 12
                    height: 1
                    color: Theme.silverFaint
                }

                Rectangle {
                    visible: !entry.modelData.separator
                    anchors.fill: parent
                    color: entry.current ? Theme.white : "transparent"
                    scale: entryMouse.pressed ? 0.97 : 1
                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }

                    NoirIcon {
                        id: entryIcon
                        visible: !!entry.modelData.icon
                        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                        size: 16
                        source: entry.modelData.icon ? entry.modelData.icon : ""
                        color: entry.current ? Theme.black : Theme.silver
                    }
                    Text {
                        anchors { left: parent.left; leftMargin: 34; verticalCenter: parent.verticalCenter }
                        text: entry.modelData.text || ""
                        color: entry.current ? Theme.black : Theme.silver
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        font.letterSpacing: Theme.letterSpacing
                    }
                }

                MouseArea {
                    id: entryMouse
                    anchors.fill: parent
                    enabled: !entry.modelData.separator
                    hoverEnabled: true
                    onEntered: list.currentIndex = entry.index
                    onClicked: menu.activate(entry.index)
                }
            }
        }
    }

    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Down:
            list.currentIndex = firstSelectable(list.currentIndex + 1, 1); break
        case Qt.Key_Up:
            list.currentIndex = firstSelectable(list.currentIndex - 1, -1); break
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
            activate(list.currentIndex); break
        case Qt.Key_Escape:
            close(); break
        default:
            return
        }
        event.accepted = true
    }
}
