import QtQuick
import QtQuick.Window
import Noir.Shell
import Noir.Ui

// Development host: the full shell (desktop, taskbar, start menu and
// notifications) composed inside one ordinary window.
Window {
    id: window

    width: shellWindowedSize.width
    height: shellWindowedSize.height
    visible: true
    title: "Noir Shell — windowed"
    color: Theme.black

    property bool startMenuOpen: false

    TaskModel { id: tasks; demo: shellDemo }
    AppModel { id: apps }

    Item {
        id: root
        anchors.fill: parent
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Super_L || event.key === Qt.Key_Super_R || event.key === Qt.Key_Meta) {
                window.startMenuOpen = !window.startMenuOpen
                event.accepted = true
            }
        }

        Desktop {
            id: desktop
            anchors.fill: parent
            focus: true
            apps: apps
            onRequestStartMenu: window.startMenuOpen = true
        }

        NotificationStack {
            anchors { right: parent.right; bottom: taskbar.top; bottomMargin: 12 }
            visible: !NoirConfig.doNotDisturb
        }

        // Click-away layer while the start menu is open.
        MouseArea {
            anchors.fill: parent
            visible: window.startMenuOpen
            onClicked: window.startMenuOpen = false
        }

        StartMenu {
            id: startMenu
            anchors { left: parent.left; leftMargin: 4; bottom: taskbar.top; bottomMargin: 4 }
            apps: apps
            opened: window.startMenuOpen
            onCloseRequested: {
                window.startMenuOpen = false
                desktop.forceActiveFocus()
            }
        }

        Taskbar {
            id: taskbar
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            tasks: tasks
            startMenuOpen: window.startMenuOpen
            onStartClicked: window.startMenuOpen = !window.startMenuOpen
            onNotificationsClicked: NotificationServer.clearAll()
        }

        Shortcut {
            sequences: ["Alt+F1", "Meta+A"]
            onActivated: window.startMenuOpen = !window.startMenuOpen
        }
    }
}
