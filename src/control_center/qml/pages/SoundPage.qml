import QtQuick
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    AudioConfig {
        id: audio
        live: page.active
        onErrorOccurred: (message) => banner.show(message)
    }

    // One device or stream: description, volume slider (to 150 %) and mute.
    component VolumeRow: SettingRow {
        id: row
        property string kind
        property var device
        label: device ? device.description : ""
        Row {
            spacing: 12
            NoirSlider {
                id: slider
                width: 260
                accessibleName: row.label + " volume"
                from: 0; to: 1.5; stepSize: 0.01
                value: row.device ? row.device.volume : 0
                enabled: row.device && !row.device.muted
                onMoved: throttle.restart()
                Timer { id: throttle; interval: 50; onTriggered: audio.setVolume(row.kind, row.device.name, slider.current) }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 44
                text: row.device && row.device.muted ? "Muted" : Math.round(slider.current * 100) + "%"
                color: slider.current > 1.0 ? Theme.white : Theme.silver
                font.pixelSize: Theme.fontNormal
            }
            NoirButton {
                width: 34; height: 30
                iconSource: Theme.icon(row.kind === "source" ? "microphone" : "volume")
                iconSize: 14
                checked: row.device ? row.device.muted : false
                toolTip: checked ? "Unmute" : "Mute"
                onClicked: audio.setMuted(row.kind, row.device.name, !row.device.muted)
            }
        }
    }

    // Default device, its volume and its port, for outputs or inputs.
    component DeviceSection: Section {
        id: section
        property string kind
        property var devices: []
        readonly property var current: devices.find(d => d.isDefault) || devices[0] || null

        SettingRow {
            separator: false
            visible: section.devices.length > 1
            label: section.kind === "sink" ? "Output device" : "Input device"
            NoirComboBox {
                width: 340
                accessibleName: section.title
                model: section.devices.map(d => d.description)
                currentIndex: section.devices.findIndex(d => d.isDefault)
                onActivated: (i) => audio.setDefault(section.kind, section.devices[i].name)
            }
        }
        VolumeRow {
            separator: section.devices.length > 1
            kind: section.kind
            device: section.current
            visible: section.current !== null
        }
        SettingRow {
            visible: section.current !== null && section.current.ports.length > 1
            label: section.kind === "sink" ? "Connector" : "Input"
            NoirComboBox {
                width: 340
                accessibleName: "Port"
                model: section.current ? section.current.ports.map(p => p.text) : []
                currentIndex: section.current ? section.current.ports.findIndex(p => p.name === section.current.activePort) : -1
                onActivated: (i) => audio.setPort(section.kind, section.current.name, section.current.ports[i].name)
            }
        }
        Text {
            visible: section.devices.length === 0
            width: parent.width
            topPadding: 6; bottomPadding: 10
            text: section.kind === "sink" ? "No output devices." : "No input devices."
            color: Theme.silverDim
            font.pixelSize: Theme.fontNormal
        }
    }

    Banner { id: banner }

    Section {
        visible: !audio.available
        title: "Sound"
        Text {
            width: parent.width
            bottomPadding: 10
            text: audio.status
            color: Theme.silver
            font.pixelSize: Theme.fontNormal
            wrapMode: Text.WordWrap
        }
    }

    DeviceSection {
        visible: audio.available
        title: "Output"
        subtitle: audio.serverName
        kind: "sink"
        devices: audio.sinks
    }

    DeviceSection {
        visible: audio.available
        title: "Input"
        kind: "source"
        devices: audio.sources
    }

    Section {
        visible: audio.available
        title: "Applications"
        subtitle: audio.streams.length ? "" : "No application is playing sound."
        Repeater {
            model: audio.streams
            VolumeRow {
                required property var modelData
                required property int index
                separator: index > 0
                kind: "sink-input"
                device: modelData
                description: modelData.detail !== modelData.description ? modelData.detail : ""
            }
        }
    }
}
