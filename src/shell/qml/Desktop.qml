import QtQuick
import Noir.Shell
import Noir.Ui

// The desktop surface: noir wallpaper, launcher icons on the left, floating
// widgets on the right and a glass context menu on right-click.
FocusScope {
    id: desktop

    property int bottomInset: Theme.taskbarHeight
    property AppModel apps

    signal requestStartMenu()

    function runCommand(command) {
        if (!apps || !apps.run(command))
            NotificationServer.post("Could not launch", command, "", 4000)
    }
    // Opens the Noir Control Center, optionally on one of its pages.
    function openControlCenter(page) {
        if (!Launcher.openControlCenter(page))
            NotificationServer.post("Could not launch", "NoirControlCenter", "", 4000)
    }

    // --- Wallpaper ---------------------------------------------------------
    // One item, so the widgets' frosted glass can show it through.
    Item {
        id: wallpaper
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#000000" }
                GradientStop { position: 0.55; color: "#07080b" }
                GradientStop { position: 1.0; color: "#14161b" }
            }
        }

        // Receding perspective grid with a silver horizon line.
        Canvas {
            id: grid
            anchors.fill: parent
            visible: NoirConfig.showWallpaperGrid
            renderStrategy: Canvas.Cooperative
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const w = width, h = height
                const horizon = h * 0.58
                const vx = w / 2

                const glow = ctx.createLinearGradient(0, horizon - 120, 0, horizon + 4)
                glow.addColorStop(0, "rgba(195,199,207,0)")
                glow.addColorStop(1, "rgba(195,199,207,0.10)")
                ctx.fillStyle = glow
                ctx.fillRect(0, horizon - 120, w, 124)

                ctx.lineWidth = 1
                // Converging verticals.
                for (let i = -24; i <= 24; ++i) {
                    const bx = vx + i * w / 14
                    ctx.strokeStyle = "rgba(195,199,207," + (0.16 - Math.abs(i) * 0.004) + ")"
                    ctx.beginPath()
                    ctx.moveTo(vx + i * 6, horizon)
                    ctx.lineTo(bx, h)
                    ctx.stroke()
                }
                // Horizontals, spaced geometrically towards the viewer.
                let y = horizon
                let step = 2
                while (y < h) {
                    const t = (y - horizon) / (h - horizon)
                    ctx.strokeStyle = "rgba(195,199,207," + (0.04 + t * 0.16) + ")"
                    ctx.beginPath()
                    ctx.moveTo(0, y)
                    ctx.lineTo(w, y)
                    ctx.stroke()
                    y += step
                    step *= 1.32
                }
                ctx.strokeStyle = "rgba(244,246,250,0.55)"
                ctx.beginPath()
                ctx.moveTo(0, horizon)
                ctx.lineTo(w, horizon)
                ctx.stroke()
            }
        }

        // Faint silver haze in the sky, where the widgets float: something for
        // their glass to catch.
        Canvas {
            anchors.fill: parent
            renderStrategy: Canvas.Cooperative
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const haze = (cx, cy, r, a) => {
                    const g = ctx.createRadialGradient(cx, cy, 0, cx, cy, r)
                    g.addColorStop(0, "rgba(195,199,207," + a + ")")
                    g.addColorStop(1, "rgba(195,199,207,0)")
                    ctx.fillStyle = g
                    ctx.fillRect(cx - r, cy - r, 2 * r, 2 * r)
                }
                haze(width * 0.82, height * 0.2, width * 0.32, 0.18)
                haze(width * 0.62, height * 0.06, width * 0.22, 0.08)
            }
        }
    }

    Text {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: desktop.bottomInset + 18 }
        text: "S I R I U S  O S"
        color: Qt.rgba(0.76, 0.78, 0.82, 0.22)
        font.family: Theme.fontFamily
        font.pixelSize: 15
        font.letterSpacing: 6
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: (mouse) => {
            desktop.forceActiveFocus()
            if (mouse.button === Qt.RightButton)
                contextMenu.popup(mouse.x, mouse.y)
            else
                contextMenu.close()
        }
    }

    // --- Launcher icons ----------------------------------------------------
    Column {
        id: icons
        x: 20
        y: 20
        spacing: 6
        visible: NoirConfig.showDesktopIcons

        Repeater {
            id: iconRepeater
            model: [
                { label: "Home", icon: Theme.icon("folder"), command: "xdg-open \"" + SystemMonitor.homePath + "\"" },
                // The terminal is looked up when activated: binding the model
                // to NoirConfig would rebuild the icons on every setting change.
                { label: "Terminal", icon: Theme.icon("terminal"), command: "@terminal" },
                { label: "Settings", icon: Theme.icon("settings"), command: "" },
                { label: "Trash", icon: Theme.icon("trash"), command: "xdg-open trash:///" },
            ]
            DesktopIcon {
                required property var modelData
                required property int index
                label: modelData.label
                iconSource: modelData.icon
                KeyNavigation.down: index + 1 < iconRepeater.count ? iconRepeater.itemAt(index + 1) : null
                KeyNavigation.up: index > 0 ? iconRepeater.itemAt(index - 1) : null
                KeyNavigation.right: clockWidget
                onActivated: {
                    if (modelData.command === "@terminal")
                        desktop.runCommand(NoirConfig.terminal)
                    else if (modelData.command.length)
                        desktop.runCommand(modelData.command)
                    else
                        desktop.openControlCenter("")
                }
            }
        }
    }

    // --- Widgets -----------------------------------------------------------
    ClockWidget {
        id: clockWidget
        visible: NoirConfig.showDesktopWidgets
        backdrop: wallpaper
        x: desktop.width - width - 36
        y: 36
        KeyNavigation.down: monitorWidget
        KeyNavigation.left: iconRepeater.itemAt(0)
    }

    SystemMonitorWidget {
        id: monitorWidget
        visible: NoirConfig.showDesktopWidgets
        backdrop: wallpaper
        x: desktop.width - width - 36
        y: clockWidget.y + clockWidget.height + 28
        KeyNavigation.up: clockWidget
        KeyNavigation.left: iconRepeater.itemAt(0)
    }

    ContextMenu {
        id: contextMenu
        items: [
            { text: "Open Terminal", icon: Theme.icon("terminal"), action: () => desktop.runCommand(NoirConfig.terminal) },
            { text: "Open Home", icon: Theme.icon("folder"), action: () => desktop.runCommand("xdg-open \"" + SystemMonitor.homePath + "\"") },
            { separator: true },
            { text: "Applications", icon: Theme.icon("start"), action: () => desktop.requestStartMenu() },
            { text: "Reduce Motion: " + (NoirConfig.reducedMotion ? "On" : "Off"), icon: Theme.icon("appearance"),
              action: () => NoirConfig.reducedMotion = !NoirConfig.reducedMotion },
            { separator: true },
            { text: "Configure Desktop…", icon: Theme.icon("appearance"), action: () => desktop.openControlCenter("appearance") },
            { text: "Display Settings…", icon: Theme.icon("display"), action: () => desktop.openControlCenter("display") },
            { text: "Control Center", icon: Theme.icon("settings"), action: () => desktop.openControlCenter("") },
            { separator: true },
            { text: "Test Notification", icon: Theme.icon("bell"),
              action: () => NotificationServer.post("Noir", "Notifications slide in from the bottom right.", "") },
        ]
    }

    // Menu key / Shift+F10 open the context menu at the desktop's top-left.
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
            contextMenu.popup(icons.x + 120, icons.y)
            event.accepted = true
        }
    }
}
