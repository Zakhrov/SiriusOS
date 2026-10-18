import QtQuick
import QtQuick.Window
import Noir.Shell
import Noir.Ui

// Real session: desktop (below everything), taskbar dock with a reserved
// strut, start menu popup and notification surface, each its own X11 window
// typed via EWMH so NoirWM can stack and composite them.
QtObject {
    id: shell

    property TaskModel tasks: TaskModel { demo: shellDemo }
    property AppModel apps: AppModel {}
    property bool startMenuOpen: false
    property double startMenuClosedAt: 0
    readonly property var screenInfo: Qt.application.screens[0]

    function toggleStartMenu() {
        // A click on the start button first deactivates (and closes) the menu;
        // don't immediately reopen it in that case.
        if (!startMenuOpen && Date.now() - startMenuClosedAt < 250)
            return
        startMenuOpen = !startMenuOpen
    }

    property Window desktopWindow: Window {
        title: "Noir Desktop"
        flags: Qt.FramelessWindowHint
        color: Theme.black
        x: shell.screenInfo.virtualX
        y: shell.screenInfo.virtualY
        width: shell.screenInfo.width
        height: shell.screenInfo.height

        Desktop {
            anchors.fill: parent
            focus: true
            apps: shell.apps
            onRequestStartMenu: shell.startMenuOpen = true
        }

        Component.onCompleted: {
            X11Integration.setupDesktop(this)
            visible = true
        }
    }

    property Window panelWindow: Window {
        title: "Noir Taskbar"
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        // Black glass over the desktop when a compositor can blend it.
        color: shellComposited ? "transparent" : Theme.black
        x: shell.screenInfo.virtualX
        y: shell.screenInfo.virtualY + shell.screenInfo.height - Theme.taskbarHeight
        width: shell.screenInfo.width
        height: Theme.taskbarHeight

        Taskbar {
            anchors.fill: parent
            tasks: shell.tasks
            startMenuOpen: shell.startMenuOpen
            onStartClicked: shell.toggleStartMenu()
            onNotificationsClicked: NotificationServer.clearAll()
        }

        Shortcut {
            sequences: ["Alt+F1", "Meta+A"]
            context: Qt.ApplicationShortcut
            onActivated: shell.toggleStartMenu()
        }

        // Frosted: the compositor blurs whatever lies behind the bar.
        readonly property bool frosted: shellComposited && Theme.glassBlur
        onFrostedChanged: X11Integration.setBlurBehind(this, frosted)

        Component.onCompleted: {
            X11Integration.setupDock(this, X11Integration.Bottom, Theme.taskbarHeight)
            X11Integration.setBlurBehind(this, frosted)
            visible = true
        }
    }

    property Window menuWindow: Window {
        id: menuWindow
        title: "Noir Applications"
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        // A transparent colour makes Qt pick an ARGB visual; only ask for
        // one when a compositing manager is there to blend it.
        color: shellComposited ? "transparent" : Theme.black
        // Extra margin on the right leaves room for the drop shadow.
        width: startMenu.width + 16
        height: startMenu.height + 4
        x: shell.panelWindow.x + 4
        y: shell.panelWindow.y - height
        visible: shell.startMenuOpen || startMenu.visible

        onVisibleChanged: if (visible) requestActivate()
        onActiveChanged: {
            if (!active && shell.startMenuOpen) {
                shell.startMenuOpen = false
                shell.startMenuClosedAt = Date.now()
            }
        }

        StartMenu {
            id: startMenu
            anchors { left: parent.left; top: parent.top }
            apps: shell.apps
            opened: shell.startMenuOpen
            onCloseRequested: shell.startMenuOpen = false
        }

        // Frosted behind the menu body (not its shadow margin), once it has
        // folded out: a blurred rectangle ahead of the fold would give it away.
        property bool frosted: false
        property Timer settle: Timer {
            interval: Theme.durationSlow
            onTriggered: menuWindow.frosted = shellComposited && Theme.glassBlur && shell.startMenuOpen
        }
        property Connections menuWatcher: Connections {
            target: shell
            function onStartMenuOpenChanged() {
                if (shell.startMenuOpen) menuWindow.settle.restart()
                else menuWindow.frosted = false
            }
        }
        onFrostedChanged: X11Integration.setBlurBehind(this, frosted, [Qt.rect(0, 0, startMenu.width, startMenu.height)])

        Component.onCompleted: X11Integration.setupPopup(this, false)
    }

    property Window notificationWindow: Window {
        title: "Noir Notifications"
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
        color: shellComposited ? "transparent" : Theme.black
        width: 400
        height: Math.max(1, stack.contentHeight + 24)
        x: shell.screenInfo.virtualX + shell.screenInfo.width - width
        y: shell.panelWindow.y - height
        // Do not disturb keeps cards off the screen; the taskbar still counts them.
        visible: !NoirConfig.doNotDisturb && (NotificationServer.count > 0 || lingering.running)

        // Keep the surface mapped while the last card animates out.
        property Timer lingering: Timer { interval: Theme.durationNormal + 50 }
        property Connections watcher: Connections {
            target: NotificationServer
            function onCountChanged() { if (NotificationServer.count === 0) shell.notificationWindow.lingering.restart() }
        }

        NotificationStack {
            id: stack
            anchors { right: parent.right; bottom: parent.bottom; bottomMargin: 12 }
        }

        // Frosted behind each card, not the gaps between them. Cards slide in
        // and out, so the region follows them while the surface is up.
        property string blurKey
        property Timer blurTracker: Timer {
            interval: 100
            repeat: true
            running: shell.notificationWindow.visible && shellComposited && Theme.glassBlur
            triggeredOnStart: true
            onTriggered: {
                const window = shell.notificationWindow
                const rects = stack.cardRects(window.contentItem)
                const key = JSON.stringify(rects)
                if (key === window.blurKey)
                    return
                window.blurKey = key
                X11Integration.setBlurBehind(window, rects.length > 0, rects)
            }
            onRunningChanged: if (!running) { shell.notificationWindow.blurKey = ""; X11Integration.setBlurBehind(shell.notificationWindow, false) }
        }

        Component.onCompleted: X11Integration.setupPopup(this, true)
    }
}
