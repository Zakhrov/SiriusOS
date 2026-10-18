import QtQuick
import QtQuick.Window
import Noir.Shell
import Noir.Ui

// Bottom taskbar: black glass bar with the start button, running tasks and
// the system tray. Arrow keys move between items; Tab leaves the bar.
FocusScope {
    id: taskbar

    property TaskModel tasks
    property bool startMenuOpen: false
    property alias startButton: start

    signal startClicked()
    signal notificationsClicked()

    height: Theme.taskbarHeight

    // Glass body (opacity from Control Center > Appearance): clearer at the
    // top, denser towards the bottom, a reflection, silver top edge + sheen.
    // With a compositor the windows behind show through, frosted.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.glassTop }
            GradientStop { position: 1.0; color: Theme.glassBottom }
        }
    }
    GlassReflection {
        anchors.fill: parent
        strength: 0.7
    }
    Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 1; color: Theme.glassEdge }
    Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 1 } height: 1; color: Theme.glassSheen }

    NoirButton {
        id: start
        anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
        width: 44; height: 40
        iconSource: Theme.icon("start")
        iconSize: 22
        checked: taskbar.startMenuOpen
        toolTip: "Applications"
        Accessible.name: "Applications"
        onClicked: taskbar.startClicked()
        KeyNavigation.right: taskList
        focus: true
    }

    Rectangle {
        id: divider
        anchors { left: start.right; leftMargin: 8; verticalCenter: parent.verticalCenter }
        width: 1; height: 28
        color: Theme.silverFaint
    }

    ListView {
        id: taskList
        anchors { left: divider.right; leftMargin: 8; right: tray.left; rightMargin: 8
                  top: parent.top; bottom: parent.bottom; topMargin: 5 }
        orientation: ListView.Horizontal
        spacing: 4
        clip: true
        interactive: false
        model: taskbar.tasks
        keyNavigationEnabled: true
        activeFocusOnTab: count > 0
        KeyNavigation.left: start

        delegate: TaskButton {
            id: taskTile
            function publishGeometry() {
                if (!taskbar || !taskbar.tasks)
                    return
                const shown = visible && x >= 0 && x + width <= taskList.width
                taskbar.tasks.setIconGeometry(index, shown ? taskTile : null)
            }
            Component.onCompleted: Qt.callLater(publishGeometry)
            onXChanged: Qt.callLater(publishGeometry)
            onYChanged: Qt.callLater(publishGeometry)
            onWidthChanged: Qt.callLater(publishGeometry)
            onHeightChanged: Qt.callLater(publishGeometry)
            onVisibleChanged: Qt.callLater(publishGeometry)
            Connections {
                target: taskList
                function onXChanged() { Qt.callLater(taskTile.publishGeometry) }
                function onYChanged() { Qt.callLater(taskTile.publishGeometry) }
                function onWidthChanged() { Qt.callLater(taskTile.publishGeometry) }
            }
            Connections {
                target: taskTile.Window.window
                function onXChanged() { Qt.callLater(taskTile.publishGeometry) }
                function onYChanged() { Qt.callLater(taskTile.publishGeometry) }
                function onVisibleChanged() { Qt.callLater(taskTile.publishGeometry) }
                function onScreenChanged() { Qt.callLater(taskTile.publishGeometry) }
            }
            title: model.title
            windowIcon: model.iconSource
            active: model.active
            minimized: model.minimized
            attention: model.attention
            focus: ListView.isCurrentItem
            onClicked: taskbar.tasks.toggle(index)
            onMiddleClicked: taskbar.tasks.close(index)
        }

        add: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationNormal }
                NumberAnimation { property: "scale"; from: 0.7; to: 1; duration: Theme.durationNormal; easing.type: Easing.OutBack }
            }
        }
        displaced: Transition {
            NumberAnimation { properties: "x"; duration: Theme.durationNormal; easing.type: Easing.OutCubic }
        }
    }

    SystemTray {
        id: tray
        anchors { right: parent.right; rightMargin: 4; verticalCenter: parent.verticalCenter }
        onNotificationsClicked: taskbar.notificationsClicked()
    }
}
