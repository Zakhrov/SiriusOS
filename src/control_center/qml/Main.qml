import QtQuick
import QtQuick.Window
import Noir.Ui
import Noir.ControlCenter

// The Control Center window: categories on the left, the selected page on
// the right. Pages swing in with a 3D turn; Ctrl+F searches, Up/Down in the
// sidebar change page, Tab moves into the page, and the page scrolls to keep
// the focused control in view. Pages that stage their changes get a footer
// with Defaults, Reset and Apply (Ctrl+S); leaving such a page, or closing
// the window, with changes not applied asks what to do with them.
Window {
    id: window

    readonly property var pages: [
        { id: "appearance", title: "Appearance", icon: "appearance", group: "Personalisation", source: "AppearancePage.qml",
          description: "Motion, glass and what the desktop shows",
          keywords: "theme animation motion speed reduce glass transparency opacity desktop icons widgets wallpaper grid" },
        { id: "notifications", title: "Notifications", icon: "bell", group: "Personalisation", source: "NotificationsPage.qml",
          description: "Pop-ups, how long they stay and do not disturb",
          keywords: "notification popup do not disturb dnd timeout quiet" },
        { id: "apps", title: "Default Applications", icon: "apps", group: "Personalisation", source: "DefaultAppsPage.qml",
          description: "Which programs open web links, files, mail and more",
          keywords: "default browser email mail terminal file manager editor image video music player mime" },
        { id: "display", title: "Display", icon: "display", group: "Hardware", source: "DisplayPage.qml",
          description: "Monitors, resolution, refresh rate, rotation and brightness",
          keywords: "display monitor screen resolution refresh rate hz rotation orientation primary brightness backlight randr" },
        { id: "sound", title: "Sound", icon: "volume", group: "Hardware", source: "SoundPage.qml",
          description: "Speakers, microphones and application volumes",
          keywords: "sound audio volume mute speaker headphones microphone input output pulseaudio pipewire" },
        { id: "input", title: "Input Devices", icon: "keyboard", group: "Hardware", source: "InputPage.qml",
          description: "Keyboard layouts and repeat, mouse and touchpad",
          keywords: "keyboard layout repeat delay rate mouse pointer speed acceleration touchpad tap scrolling natural left handed" },
        { id: "power", title: "Power", icon: "battery", group: "Hardware", source: "PowerPage.qml",
          description: "Battery, power profile, screen blanking, suspend and shut down",
          keywords: "power battery charge profile performance saver screen blank dpms suspend sleep hibernate restart shutdown" },
        { id: "graphics", title: "Graphics", icon: "gpu", group: "Hardware", source: "GraphicsPage.qml",
          description: "Vulkan devices, the GPU Noir renders with, and the compositor",
          keywords: "graphics gpu vulkan driver compositor validation renderer device" },
        { id: "network", title: "Network", icon: "wifi", group: "Connectivity", source: "NetworkPage.qml",
          description: "Wired and Wi-Fi connections",
          keywords: "network wifi wireless ethernet wired ip address connection internet password networkmanager" },
        { id: "bluetooth", title: "Bluetooth", icon: "bluetooth", group: "Connectivity", source: "BluetoothPage.qml",
          description: "Pair and connect Bluetooth devices",
          keywords: "bluetooth pair connect headset headphones keyboard mouse wireless device" },
        { id: "datetime", title: "Date & Time", icon: "clock", group: "System", source: "DateTimePage.qml",
          description: "Time zone, network time and the clock format",
          keywords: "date time clock timezone zone ntp network time 24 hour seconds format" },
        { id: "about", title: "About This System", icon: "info", group: "System", source: "AboutPage.qml",
          description: "The operating system and the hardware it runs on",
          keywords: "about system information hardware processor cpu memory ram gpu graphics disk storage kernel version" },
    ]

    property string currentId: pageById(ControlCenterService.initialPage) ? ControlCenterService.initialPage : "appearance"
    readonly property var currentPage: pageById(currentId)

    function pageById(id) {
        for (let i = 0; i < pages.length; ++i) {
            if (pages[i].id === id)
                return pages[i]
        }
        return null
    }
    readonly property var page: pageLoader.item
    // Message strip above the footer, for pages to use (host.footerBanner).
    property alias footerBanner: footerBanner

    // Runs `then` once the current page's unapplied changes are dealt with.
    function settleChanges(then) {
        if (page && page.busy) {
            footerBanner.show(page.busyText, "info")
            return
        }
        if (!page || !page.hasChanges) {
            then()
            return
        }
        footerBanner.show("Apply the changes to " + currentPage.title + "?", "info", [
            { text: "Apply", action: () => { footerBanner.hide(); window.page.apply(); then() } },
            { text: "Discard", action: () => { footerBanner.hide(); window.page.reset(); then() } },
            { text: "Cancel", action: () => footerBanner.hide() },
        ])
    }
    function showPage(id) {
        if (pageById(id) && id !== currentId)
            settleChanges(() => { footerBanner.hide(); currentId = id })
    }

    // Closing with changes pending asks first.
    property bool closeConfirmed: false
    onClosing: (close) => {
        if (closeConfirmed || !page || (!page.hasChanges && !page.busy))
            return
        close.accepted = false
        settleChanges(() => { window.closeConfirmed = true; window.close() })
    }

    title: currentPage ? currentPage.title + " — Control Center" : "Control Center"
    // Comfortable size, but never larger than the screen's free area.
    width: Math.min(1180, Screen.desktopAvailableWidth - 40)
    height: Math.min(780, Screen.desktopAvailableHeight - 40)
    minimumWidth: 880
    minimumHeight: 560
    visible: true
    color: Theme.black

    Connections {
        target: ControlCenterService
        function onShowRequested(page) {
            window.showPage(page)
            window.show()
            window.raise()
            window.requestActivate()
        }
    }

    Shortcut { sequences: ["Ctrl+F", "/"]; onActivated: sidebar.focusSearch() }
    Shortcut { sequences: [StandardKey.Close, StandardKey.Quit]; onActivated: window.close() }
    Shortcut {
        sequences: [StandardKey.Save]
        enabled: !!window.page && window.page.staged && window.page.hasChanges
        onActivated: window.page.apply()
    }

    // Keep whatever has keyboard focus inside the page in view.
    onActiveFocusItemChanged: {
        const item = activeFocusItem
        if (!item || !pageLoader.item)
            return
        let p = item
        while (p && p !== pageLoader.item)
            p = p.parent
        if (!p)
            return
        const top = item.mapToItem(flick.contentItem, 0, 0).y
        const bottom = top + item.height
        if (top - 24 < flick.contentY)
            flick.contentY = Math.max(0, top - 24)
        else if (bottom + 24 > flick.contentY + flick.height)
            flick.contentY = Math.min(flick.contentHeight - flick.height, bottom + 24 - flick.height)
    }

    // --- Backdrop ------------------------------------------------------------
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#000000" }
            GradientStop { position: 1.0; color: "#101216" }
        }
    }

    Sidebar {
        id: sidebar
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 12 }
        width: 290
        pages: window.pages
        currentId: window.currentId
        focus: true
        onSelected: (id) => window.showPage(id)
        onEnterPage: pageLoader.item && pageLoader.item.nextItemInFocusChain(true)
                     ? pageLoader.item.nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocusReason) : null
    }

    // --- Page ----------------------------------------------------------------
    Item {
        id: content
        anchors { left: sidebar.right; right: parent.right; top: parent.top; bottom: parent.bottom
                  leftMargin: 20; rightMargin: 12; topMargin: 12; bottomMargin: 12 }

        Row {
            id: header
            anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 8 }
            spacing: 16
            height: 60

            Rectangle {
                width: 52; height: 52
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.abyss
                border.width: Theme.edge
                border.color: Theme.silver
                Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 } height: 1; color: Qt.rgba(1, 1, 1, 0.3) }
                NoirIcon {
                    anchors.centerIn: parent
                    source: window.currentPage ? Theme.icon(window.currentPage.icon) : ""
                    size: 28
                    color: Theme.white
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text {
                    text: window.currentPage ? window.currentPage.title : ""
                    color: Theme.white
                    font.family: Theme.fontFamily
                    font.pixelSize: 26
                    font.weight: Font.Light
                    font.letterSpacing: 1.5
                }
                Text {
                    text: window.currentPage ? window.currentPage.description : ""
                    color: Theme.silverDim
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontNormal
                }
            }
        }

        Rectangle {
            id: rule
            anchors { left: parent.left; right: parent.right; top: header.bottom; topMargin: 14 }
            height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Theme.silver }
                GradientStop { position: 1; color: "transparent" }
            }
        }

        Flickable {
            id: flick
            anchors { left: parent.left; right: scrollBar.left; top: rule.bottom; bottom: footerBanner.top; topMargin: 2; rightMargin: 6 }
            clip: true
            contentWidth: width
            contentHeight: pageHost.height + 40
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick

            Item {
                id: pageHost
                x: 0
                y: 18
                width: flick.width - 8
                height: pageLoader.item ? pageLoader.item.height : 0
                transform: Rotation {
                    id: swing
                    origin.x: 0
                    origin.y: Math.min(pageHost.height, flick.height) / 2
                    axis { x: 0; y: 1; z: 0 }
                }

                Loader {
                    id: pageLoader
                    width: parent.width
                    source: window.currentPage ? window.currentPage.source : ""
                    // A Loader does not say why a page failed; the component does.
                    onStatusChanged: if (status === Loader.Error) console.warn(Qt.createComponent(source).errorString())
                    onLoaded: {
                        item.host = window
                        flick.contentY = 0
                        if (!Theme.reducedMotion)
                            enter.restart()
                    }
                }
            }

            ParallelAnimation {
                id: enter
                NumberAnimation { target: swing; property: "angle"; from: -14; to: 0; duration: Theme.durationSlow; easing.type: Easing.OutCubic }
                NumberAnimation { target: pageHost; property: "opacity"; from: 0; to: 1; duration: Theme.durationNormal }
                NumberAnimation { target: pageHost; property: "x"; from: 28; to: 0; duration: Theme.durationSlow; easing.type: Easing.OutCubic }
            }
        }

        // --- Footer: Defaults / Reset / Apply for staged pages ---------------
        Banner {
            id: footerBanner
            anchors { left: parent.left; right: parent.right; bottom: footer.top; bottomMargin: shown ? 10 : 0 }
            autoHideMs: 5000
        }

        Item {
            id: footer
            readonly property bool staged: !!window.page && window.page.staged
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: staged ? 58 : 0
            visible: staged
            clip: true

            Rectangle {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: Theme.silverDim }
                    GradientStop { position: 1; color: "transparent" }
                }
            }
            Text {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter; verticalCenterOffset: 4 }
                text: !window.page ? "" : window.page.busy ? window.page.busyText
                    : window.page.hasChanges ? "Changes not applied yet" : "All changes applied"
                color: window.page && window.page.hasChanges ? Theme.white : Theme.silverDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
            }
            Row {
                anchors { right: parent.right; verticalCenter: parent.verticalCenter; verticalCenterOffset: 4 }
                spacing: 8
                NoirButton {
                    text: "Defaults"
                    height: 36
                    toolTip: "Set this page back to the Noir defaults (then Apply)"
                    enabled: !!window.page && Object.keys(window.page.defaults).length > 0
                             && !window.page.atDefaults && !window.page.busy
                    onClicked: window.page.restoreDefaults()
                }
                NoirButton {
                    text: "Reset"
                    height: 36
                    toolTip: "Undo the changes not applied yet"
                    enabled: !!window.page && window.page.hasChanges && !window.page.busy
                    onClicked: window.page.reset()
                }
                NoirButton {
                    text: "Apply"
                    height: 36
                    toolTip: "Apply the changes (Ctrl+S)"
                    iconSource: Theme.icon("check")
                    iconSize: 14
                    enabled: !!window.page && window.page.hasChanges && !window.page.busy
                    // The call to action stands out while there is something to apply.
                    checked: enabled
                    onClicked: window.page.apply()
                }
            }
        }

        NoirScrollBar {
            id: scrollBar
            anchors { right: parent.right; top: flick.top; bottom: flick.bottom; topMargin: 18 }
            flickable: flick
        }

        Text {
            anchors.centerIn: flick
            visible: pageLoader.status === Loader.Error
            text: "This page could not be loaded."
            color: Theme.silverDim
            font.pixelSize: Theme.fontNormal
        }
    }
}
