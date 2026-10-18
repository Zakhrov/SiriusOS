import QtQuick
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    BluetoothConfig {
        id: bluetooth
        onErrorOccurred: (message) => banner.show(message)
    }

    // Faster while searching, so new devices show up quickly.
    Timer {
        interval: bluetooth.discovering ? 1500 : 4000
        repeat: true
        running: page.active && bluetooth.available
        onTriggered: bluetooth.refresh()
    }
    onActiveChanged: if (!active && bluetooth.discovering) bluetooth.setDiscovering(false)

    Banner { id: banner }

    Section {
        title: "Bluetooth"
        subtitle: bluetooth.available ? bluetooth.adapterName + " · " + bluetooth.adapterAddress : bluetooth.status

        SettingRow {
            separator: false
            visible: bluetooth.available
            label: "Bluetooth"
            description: bluetooth.powered ? "On" : "Off"
            NoirSwitch {
                accessibleName: "Bluetooth"
                enabled: !bluetooth.busy
                checked: bluetooth.powered
                onToggled: (on) => bluetooth.setPowered(on)
            }
        }
        SettingRow {
            visible: bluetooth.available
            dimmed: !bluetooth.powered
            label: "Search for devices"
            description: bluetooth.discovering ? "Searching… put the device in pairing mode." : "Find devices that are ready to pair."
            NoirSwitch {
                accessibleName: "Search for devices"
                enabled: bluetooth.powered
                checked: bluetooth.discovering
                onToggled: (on) => bluetooth.setDiscovering(on)
            }
        }
    }

    Section {
        visible: bluetooth.available && bluetooth.powered
        title: "Devices"
        subtitle: bluetooth.devices.length ? "" : "No devices yet. Turn on searching to find some."

        Repeater {
            model: bluetooth.devices
            SettingRow {
                id: row
                required property var modelData
                required property int index
                separator: index > 0
                label: modelData.name
                description: [modelData.kind,
                              modelData.connected ? "Connected" : (modelData.paired ? "Paired" : "Not paired"),
                              modelData.battery !== undefined && modelData.battery !== null ? "Battery " + modelData.battery + "%" : ""]
                             .filter(s => s.length).join(" · ")
                Row {
                    spacing: 6
                    NoirButton {
                        visible: row.modelData.paired
                        text: "Remove"
                        enabled: !bluetooth.busy
                        onClicked: bluetooth.removeDevice(row.modelData.path)
                    }
                    NoirButton {
                        text: row.modelData.connected ? "Disconnect" : (row.modelData.paired ? "Connect" : "Pair")
                        enabled: !bluetooth.busy
                        onClicked: {
                            if (row.modelData.connected) bluetooth.disconnectDevice(row.modelData.path)
                            else if (row.modelData.paired) bluetooth.connectDevice(row.modelData.path)
                            else bluetooth.pair(row.modelData.path)
                        }
                    }
                }
            }
        }
    }
}
