import QtQuick
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    property string expanded        // SSID whose password field is open

    NetworkConfig {
        id: network
        onErrorOccurred: (message) => banner.show(message)
        onMessage: (text) => banner.show(text, "info")
    }

    Timer {
        interval: 5000
        repeat: true
        running: page.active && network.managerAvailable
        onTriggered: network.refresh()
    }

    // Four rising bars, lit according to the signal strength.
    component SignalBars: Row {
        property int strength
        property color color: Theme.silver
        spacing: 2
        Repeater {
            model: 4
            Rectangle {
                required property int index
                anchors.bottom: parent.bottom
                width: 4
                height: 5 + index * 4
                color: parent.color
                opacity: parent.strength > index * 25 ? 1 : 0.2
            }
        }
    }

    Banner { id: banner }

    Banner {
        id: noManager
        shown: !network.managerAvailable
        kind: "info"
        text: network.status
        autoHideMs: 2147483647
    }

    Section {
        visible: network.managerAvailable
        title: "Connections"

        SettingRow {
            separator: false
            label: "Networking"
            description: "All network devices."
            NoirSwitch {
                accessibleName: "Networking"
                enabled: !network.busy
                checked: network.networkingEnabled
                onToggled: (on) => network.setNetworkingEnabled(on)
            }
        }
        SettingRow {
            visible: network.hasWifi
            label: "Wi-Fi"
            description: network.wirelessHardwareEnabled ? "" : "Switched off by a hardware switch or airplane mode."
            NoirSwitch {
                accessibleName: "Wi-Fi"
                enabled: !network.busy && network.wirelessHardwareEnabled && network.networkingEnabled
                checked: network.wirelessEnabled
                onToggled: (on) => network.setWirelessEnabled(on)
            }
        }
    }

    Section {
        title: "Devices"
        subtitle: network.devices.length ? "" : "No network devices."

        Repeater {
            model: network.devices
            Item {
                id: device
                required property var modelData
                required property int index
                width: parent.width
                height: info.implicitHeight + 24

                Rectangle {
                    visible: device.index > 0
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: 1
                    color: Theme.silverFaint
                }
                NoirIcon {
                    id: glyph
                    anchors { left: parent.left; top: parent.top; topMargin: 14 }
                    source: Theme.icon(device.modelData.type === "Wi-Fi" ? "wifi" : "network")
                    size: 20
                    color: device.modelData.connected ? Theme.white : Theme.silverDim
                }
                Column {
                    id: info
                    anchors { left: glyph.right; leftMargin: 14; right: action.left; rightMargin: 12; top: parent.top; topMargin: 12 }
                    spacing: 3
                    Text {
                        text: device.modelData.type + " · " + device.modelData.interface
                        color: Theme.white
                        font.pixelSize: Theme.fontNormal
                    }
                    Text {
                        text: device.modelData.state + (device.modelData.connection ? " — " + device.modelData.connection : "")
                              + (device.modelData.speed ? " · " + device.modelData.speed : "")
                        color: device.modelData.connected ? Theme.silver : Theme.silverDim
                        font.pixelSize: Theme.fontSmall
                    }
                    Text {
                        visible: text.length > 0
                        width: parent.width
                        text: device.modelData.ipv4.concat(device.modelData.ipv6).join("   ")
                        color: Theme.silverDim
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WrapAnywhere
                    }
                    Text {
                        visible: !!device.modelData.mac
                        text: "MAC " + device.modelData.mac
                        color: Theme.silverDim
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }
                NoirButton {
                    id: action
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    visible: network.managerAvailable && device.modelData.connected && device.modelData.path.length > 0
                    text: "Disconnect"
                    enabled: !network.busy
                    onClicked: network.disconnectDevice(device.modelData.path)
                }
            }
        }
    }

    Section {
        visible: network.managerAvailable && network.hasWifi && network.wirelessEnabled
        title: "Wi-Fi networks"
        actions: [
            NoirButton {
                text: "Scan"
                iconSource: Theme.icon("refresh")
                iconSize: 14
                enabled: !network.busy
                onClicked: network.scan()
            }
        ]

        Text {
            visible: network.accessPoints.length === 0
            topPadding: 6; bottomPadding: 12
            text: "No networks in range."
            color: Theme.silverDim
            font.pixelSize: Theme.fontNormal
        }

        Repeater {
            model: network.accessPoints
            Item {
                id: ap
                required property var modelData
                required property int index
                readonly property bool open: page.expanded === modelData.ssid
                width: parent.width
                height: 52 + (open ? 48 : 0)
                clip: true
                Behavior on height { NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic } }

                Rectangle {
                    visible: ap.index > 0
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: 1
                    color: Theme.silverFaint
                }
                SignalBars {
                    id: bars
                    anchors { left: parent.left; top: parent.top; topMargin: 17 }
                    strength: ap.modelData.strength
                    color: ap.modelData.active ? Theme.white : Theme.silver
                }
                NoirIcon {
                    id: lock
                    anchors { left: bars.right; leftMargin: 10; verticalCenter: bars.verticalCenter }
                    source: Theme.icon("lock")
                    size: 12
                    color: Theme.silverDim
                    opacity: ap.modelData.secured ? 1 : 0
                }
                Column {
                    anchors { left: lock.right; leftMargin: 12; right: buttons.left; rightMargin: 12; top: parent.top; topMargin: 9 }
                    Text {
                        width: parent.width
                        text: ap.modelData.ssid
                        color: Theme.white
                        font.pixelSize: Theme.fontNormal
                        elide: Text.ElideRight
                    }
                    Text {
                        text: [ap.modelData.active ? "Connected" : (ap.modelData.saved ? "Saved" : ""),
                               ap.modelData.secured ? (ap.modelData.enterprise ? "Enterprise" : "Secured") : "Open",
                               (ap.modelData.frequency > 4900 ? "5 GHz" : "2.4 GHz"), ap.modelData.strength + "%"]
                              .filter(s => s.length).join(" · ")
                        color: Theme.silverDim
                        font.pixelSize: Theme.fontSmall
                    }
                }
                Row {
                    id: buttons
                    anchors { right: parent.right; top: parent.top; topMargin: 10 }
                    spacing: 6
                    NoirButton {
                        visible: ap.modelData.saved
                        text: "Forget"
                        enabled: !network.busy
                        onClicked: network.forgetConnection(ap.modelData.uuid)
                    }
                    NoirButton {
                        visible: !ap.modelData.active
                        text: "Connect"
                        enabled: !network.busy
                        onClicked: {
                            if (ap.modelData.secured && !ap.modelData.saved && !ap.modelData.enterprise) {
                                page.expanded = ap.open ? "" : ap.modelData.ssid
                                if (page.expanded.length)
                                    password.forceActiveFocus()
                            } else {
                                network.connectToNetwork(ap.modelData.ssid, "")
                            }
                        }
                    }
                }
                Row {
                    anchors { right: parent.right; top: parent.top; topMargin: 56 }
                    spacing: 8
                    visible: ap.open
                    NoirTextField {
                        id: password
                        width: 280
                        echoMode: TextInput.Password
                        placeholderText: "Password for " + ap.modelData.ssid
                        iconSource: Theme.icon("lock")
                        onAccepted: join.clicked()
                    }
                    NoirButton {
                        id: join
                        text: "Join"
                        enabled: password.text.length >= 8 && !network.busy
                        onClicked: {
                            network.connectToNetwork(ap.modelData.ssid, password.text)
                            password.text = ""
                            page.expanded = ""
                        }
                    }
                }
            }
        }
    }
}
