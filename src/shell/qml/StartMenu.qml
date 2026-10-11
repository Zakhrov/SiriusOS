import QtQuick
import Noir.Shell

// Application launcher. Folds up out of the taskbar (hinged on its bottom
// edge), type-to-search, arrow keys to move, Enter to launch, Esc to close.
FocusScope {
    id: menu

    property AppModel apps
    property bool opened: false

    signal closeRequested()

    width: 560
    height: 540
    visible: opened || body.opacity > 0.01

    onOpenedChanged: {
        if (opened) {
            search.text = ""
            appList.currentIndex = 0
            search.forceActiveFocus()
        }
    }

    function launch(row) {
        if (row < 0)
            return
        apps.launch(filter.sourceRow(row))
        closeRequested()
    }

    AppFilterModel {
        id: filter
        sourceModel: menu.apps
        query: search.text
        onQueryChanged: appList.currentIndex = 0
    }

    Item {
        id: body
        anchors.fill: parent
        opacity: menu.opened ? 1 : 0
        transform: [
            Rotation {
                id: fold
                origin.x: body.width / 2
                origin.y: body.height
                axis { x: 1; y: 0; z: 0 }
                angle: menu.opened ? 0 : -Theme.perspectiveTilt
                Behavior on angle { NumberAnimation { duration: Theme.durationSlow; easing.type: Easing.OutCubic } }
            },
            Translate {
                y: menu.opened ? 0 : 24
                Behavior on y { NumberAnimation { duration: Theme.durationSlow; easing.type: Easing.OutCubic } }
            }
        ]
        Behavior on opacity { NumberAnimation { duration: Theme.durationNormal } }

        GlassPanel {
            anchors.fill: parent
            color: Theme.glassStrong
            elevation: 14
        }

        // --- Side rail: identity + power --------------------------------------
        Rectangle {
            id: rail
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 1 }
            width: 56
            color: Qt.rgba(0, 0, 0, 0.45)
            Rectangle { anchors { right: parent.right; top: parent.top; bottom: parent.bottom } width: 1; color: Theme.silverFaint }

            Rectangle {
                anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 12 }
                width: 36; height: 36
                color: Theme.black
                border.color: Theme.silver
                Text {
                    anchors.centerIn: parent
                    text: SystemMonitor.userName.charAt(0).toUpperCase()
                    color: Theme.white
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontLarge
                }
            }

            NoirButton {
                id: powerButton
                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 10 }
                width: 40; height: 40
                flat: true
                iconSource: "icons/noir-power.svg"
                iconSize: 20
                toolTip: "Power"
                onClicked: powerMenu.popup(rail.width + 4, menu.height - powerMenu.height - 10)
                KeyNavigation.up: search
            }
        }

        // --- Search ---------------------------------------------------------
        Item {
            id: header
            anchors { left: rail.right; right: parent.right; top: parent.top; margins: 14 }
            height: 40

            Rectangle {
                anchors.fill: parent
                color: Theme.abyss
                border.width: 1
                border.color: search.activeFocus ? Theme.white : Theme.silverFaint
            }
            Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 } height: 1; color: Qt.rgba(0, 0, 0, 0.9) }

            NoirIcon {
                id: searchIcon
                anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                size: 16
                source: "icons/noir-search.svg"
                color: Theme.silverDim
            }
            TextInput {
                id: search
                anchors { left: searchIcon.right; leftMargin: 10; right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                color: Theme.white
                selectionColor: Theme.white
                selectedTextColor: Theme.black
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal + 1
                font.letterSpacing: Theme.letterSpacing
                clip: true
                Accessible.role: Accessible.EditableText
                Accessible.name: "Search applications"

                Text {
                    visible: !search.text.length
                    text: "Search applications…"
                    color: Theme.silverDim
                    font: search.font
                }

                Keys.onPressed: (event) => {
                    switch (event.key) {
                    case Qt.Key_Down: appList.incrementCurrentIndex(); break
                    case Qt.Key_Up: appList.decrementCurrentIndex(); break
                    case Qt.Key_PageDown: appList.currentIndex = Math.min(appList.count - 1, appList.currentIndex + 8); break
                    case Qt.Key_PageUp: appList.currentIndex = Math.max(0, appList.currentIndex - 8); break
                    case Qt.Key_Return:
                    case Qt.Key_Enter: menu.launch(appList.currentIndex); break
                    case Qt.Key_Escape: menu.closeRequested(); break
                    default: return
                    }
                    event.accepted = true
                }
                KeyNavigation.tab: powerButton
            }
        }

        // --- Application list -------------------------------------------------
        Text {
            id: sectionLabel
            anchors { left: header.left; top: header.bottom; topMargin: 14 }
            text: (search.text.length ? "RESULTS" : "ALL APPLICATIONS") + "  ·  " + filter.count
            color: Theme.silverDim
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            font.letterSpacing: 2.5
        }

        ListView {
            id: appList
            anchors { left: header.left; right: header.right; top: sectionLabel.bottom; topMargin: 8
                      bottom: parent.bottom; bottomMargin: 14 }
            clip: true
            model: filter
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: Theme.durationFast
            highlightFollowsCurrentItem: true
            highlight: Rectangle { color: Theme.white }

            delegate: Item {
                id: entry
                required property int index
                required property string name
                required property string comment
                required property string iconName
                readonly property bool current: ListView.isCurrentItem
                width: appList.width - scrollBar.width - 4
                height: 44
                scale: entryMouse.pressed ? 0.98 : 1

                Rectangle {
                    anchors.centerIn: appIcon
                    width: 32; height: 32
                    color: Theme.black
                    border.color: entry.current ? Theme.black : Theme.silverFaint
                }
                Image {
                    id: appIcon
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 24; height: 24
                    sourceSize: Qt.size(48, 48)
                    source: entry.iconName.length ? "image://appicon/" + encodeURIComponent(entry.iconName) : ""
                    visible: status === Image.Ready && implicitWidth > 1
                }
                NoirIcon {
                    anchors.centerIn: appIcon
                    visible: !appIcon.visible
                    size: 18
                    source: "icons/noir-app.svg"
                }
                Column {
                    anchors { left: appIcon.right; leftMargin: 16; right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    Text {
                        width: parent.width
                        text: entry.name
                        elide: Text.ElideRight
                        color: entry.current ? Theme.black : Theme.white
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        font.letterSpacing: 0.4
                    }
                    Text {
                        width: parent.width
                        visible: text.length > 0
                        text: entry.comment
                        elide: Text.ElideRight
                        color: entry.current ? Theme.silverFaint : Theme.silverDim
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }
                MouseArea {
                    id: entryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: appList.currentIndex = entry.index
                    onClicked: menu.launch(entry.index)
                }
            }

            // 3D groove scrollbar.
            Item {
                id: scrollBar
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                width: 6
                visible: appList.contentHeight > appList.height
                Rectangle { anchors.fill: parent; color: Theme.abyss; border.color: Theme.silverFaint }
                Rectangle {
                    x: 1
                    width: parent.width - 2
                    y: appList.visibleArea.yPosition * parent.height
                    height: Math.max(20, appList.visibleArea.heightRatio * parent.height)
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: Theme.white }
                        GradientStop { position: 1; color: Theme.silverDim }
                    }
                    Behavior on y { NumberAnimation { duration: Theme.durationFast } }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: appList.count === 0
                text: menu.apps && menu.apps.count === 0 ? "No applications installed" : "No matches"
                color: Theme.silverDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
            }
        }

        ContextMenu {
            id: powerMenu
            width: 180
            items: [
                { text: "Lock", icon: "icons/noir-power.svg", action: () => menu.apps.run("loginctl lock-session") },
                { text: "Log Out", icon: "icons/noir-power.svg", action: () => Qt.quit() },
                { separator: true },
                { text: "Restart", icon: "icons/noir-power.svg", action: () => menu.apps.run("systemctl reboot") },
                { text: "Shut Down", icon: "icons/noir-power.svg", action: () => menu.apps.run("systemctl poweroff") },
            ]
        }
    }
}
