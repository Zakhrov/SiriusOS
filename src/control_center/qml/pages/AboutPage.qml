import QtQuick
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    SystemInfo { id: info }

    Banner { id: banner }

    // Hero: the system's name over a horizon line, like the wallpaper.
    Item {
        width: parent.width
        height: 150

        Rectangle {
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; verticalCenterOffset: 30 }
            height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 0.5; color: Theme.white }
                GradientStop { position: 1; color: "transparent" }
            }
        }
        Column {
            anchors.centerIn: parent
            spacing: 8
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "S I R I U S   O S"
                color: Theme.white
                font.family: Theme.fontFamily
                font.pixelSize: 30
                font.weight: Font.Light
                font.letterSpacing: 6
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Noir Desktop Environment " + info.noirVersion
                color: Theme.silver
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                font.letterSpacing: 2
            }
            Item { width: 1; height: 18 }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: info.osName
                color: Theme.silverDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                font.letterSpacing: 1.5
            }
        }
    }

    Section {
        title: "Software"
        actions: [
            NoirButton {
                text: "Copy details"
                iconSource: Theme.icon("apps")
                iconSize: 14
                onClicked: {
                    info.copySummary()
                    banner.show("System details copied to the clipboard.", "info")
                }
            }
        ]
        InfoRow { label: "Operating system"; value: info.osName }
        InfoRow { label: "Desktop"; value: "Noir " + info.noirVersion + " · Qt " + info.qtVersion }
        InfoRow { label: "Kernel"; value: info.kernel + " (" + info.architecture + ")" }
        InfoRow { label: "Window system"; value: info.windowSystem }
        InfoRow { label: "Computer name"; value: info.hostName }
        InfoRow { label: "Uptime"; value: info.uptime }
        Item { width: 1; height: 8 }
    }

    Section {
        title: "Hardware"
        InfoRow { label: "Machine"; value: info.machine }
        InfoRow { label: "Virtual machine"; value: info.virtualization }
        InfoRow { label: "Processor"; value: info.cpuModel + (info.cpuThreads ? " — " + info.cpuCores + " cores, " + info.cpuThreads + " threads" : "") }
        InfoRow { label: "Memory"; value: info.memory }
        Repeater {
            model: info.gpus
            InfoRow {
                required property var modelData
                required property int index
                label: index === 0 ? "Graphics" : ""
                value: modelData.name + " — " + modelData.driverName + ", Vulkan " + modelData.apiVersion + (modelData.active ? "  (in use)" : "")
            }
        }
        Repeater {
            model: info.screens
            InfoRow {
                required property var modelData
                required property int index
                label: index === 0 ? "Displays" : ""
                value: modelData.name + " · " + modelData.resolution + " @ " + modelData.refresh + (modelData.model ? " · " + modelData.model : "")
            }
        }
        Item {
            width: parent.width
            height: 44
            NoirButton {
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                text: "Graphics settings"
                iconSource: Theme.icon("chevron-right")
                iconSize: 12
                onClicked: page.host.showPage("graphics")
            }
        }
    }

    Section {
        title: "Storage"
        subtitle: info.disks.length ? "" : "No local disks."

        Repeater {
            model: info.disks
            Item {
                id: disk
                required property var modelData
                required property int index
                width: parent.width
                height: 64

                Rectangle {
                    visible: disk.index > 0
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: 1
                    color: Theme.silverFaint
                }
                Text {
                    anchors { left: parent.left; top: parent.top; topMargin: 10 }
                    text: disk.modelData.mount + "   " + disk.modelData.device + " · " + disk.modelData.fileSystem
                    color: Theme.white
                    font.pixelSize: Theme.fontNormal
                }
                Text {
                    anchors { right: parent.right; top: parent.top; topMargin: 10 }
                    text: disk.modelData.free + " free of " + disk.modelData.total
                    color: Theme.silver
                    font.pixelSize: Theme.fontSmall
                }
                ProgressBar3D {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 12 }
                    height: 10
                    value: disk.modelData.used
                }
            }
        }
    }
}
