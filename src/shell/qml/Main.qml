import QtQml
import Noir.Shell

// Entry point: builds either the real session shell (one X11 surface per
// role) or the single-window development shell (--windowed).
QtObject {
    id: main

    property var shell: {
        const component = Qt.createComponent(shellWindowed ? "WindowedShell.qml" : "SessionShell.qml")
        if (component.status === Component.Error) {
            console.error(component.errorString())
            Qt.exit(1)
            return null
        }
        return component.createObject(main)
    }

    // --demo: seed a few notifications so every surface has content.
    property Timer demoTimer: Timer {
        running: shellDemo
        interval: 900
        onTriggered: {
            NotificationServer.send("Noir Update", "System update ready",
                                    "SiriusOS 0.1 components downloaded.", "", [], { value: 72 }, 0)
            NotificationServer.send("Terminal", "Build finished",
                                    "NoirShell built in 4.2s with 0 warnings.", "utilities-terminal",
                                    ["open", "Open", "later", "Later"], {}, 0)
        }
    }
}
