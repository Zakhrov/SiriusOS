import QtQuick
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    readonly property var choices: [""].concat(graphics.devices.filter(d => d.presents).map(d => d.name))
    readonly property string reasonText: ({
        "NOIR_VK_DEVICE": "chosen in the preferences below",
        "drives the X server": "it drives this display",
        "preferred device type": "the best device available",
    })[graphics.activeReason] || graphics.activeReason

    staged: true
    defaults: ({ preferredDevice: "", compositorEnabled: true, validationEnabled: false })
    function current(key) { return graphics[key] }
    function commit(key, value) {
        if (key === "preferredDevice") graphics.setPreferredDevice(value)
        else if (key === "compositorEnabled") graphics.setCompositorEnabled(value)
        else if (key === "validationEnabled") graphics.setValidationEnabled(value)
    }
    onCommitted: banner.show("Saved. Log out and back in for these to take effect.", "info")

    GraphicsConfig {
        id: graphics
        onErrorOccurred: (message) => banner.show(message)
    }

    Banner { id: banner }

    Section {
        title: "Renderer"
        subtitle: "The whole Noir stack — window manager, shell and Control Center — renders with Vulkan 1.0 on the same device."

        InfoRow { label: "Rendering on"; value: graphics.activeDevice.length ? graphics.activeDevice + " (" + page.reasonText + ")" : "No Vulkan device can draw to this display" }
        InfoRow { label: "Vulkan loader"; value: graphics.loaderVersion }
        Item { width: 1; height: 8 }
    }

    Section {
        title: "Vulkan devices"

        Repeater {
            model: graphics.devices
            Item {
                id: gpu
                required property var modelData
                required property int index
                width: parent.width
                height: details.implicitHeight + 24

                Rectangle {
                    visible: gpu.index > 0
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: 1
                    color: Theme.silverFaint
                }
                NoirIcon {
                    id: glyph
                    anchors { left: parent.left; top: parent.top; topMargin: 14 }
                    source: Theme.icon("gpu")
                    size: 22
                    color: gpu.modelData.active ? Theme.white : Theme.silverDim
                }
                Column {
                    id: details
                    anchors { left: glyph.right; leftMargin: 14; right: badge.left; rightMargin: 12; top: parent.top; topMargin: 12 }
                    spacing: 3
                    Text { text: gpu.modelData.name; color: Theme.white; font.pixelSize: Theme.fontNormal }
                    Text {
                        text: gpu.modelData.typeName + " · " + gpu.modelData.vendor + " · Vulkan " + gpu.modelData.apiVersion
                        color: Theme.silver
                        font.pixelSize: Theme.fontSmall
                    }
                    Text {
                        text: gpu.modelData.driverName + (gpu.modelData.driverInfo ? " " + gpu.modelData.driverInfo : "") + " · " + gpu.modelData.deviceId
                        color: Theme.silverDim
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fontSmall
                    }
                    Text {
                        text: gpu.modelData.presents ? "Can draw to this display" : "Cannot draw to this display"
                        color: Theme.silverDim
                        font.pixelSize: Theme.fontSmall
                    }
                }
                Rectangle {
                    id: badge
                    anchors { right: parent.right; top: parent.top; topMargin: 14 }
                    visible: gpu.modelData.active
                    width: badgeText.implicitWidth + 16
                    height: 22
                    color: Theme.white
                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: "IN USE"
                        color: Theme.black
                        font.pixelSize: Theme.fontSmall - 1
                        font.letterSpacing: 2
                    }
                }
            }
        }
        Text {
            visible: graphics.devices.length === 0
            topPadding: 6; bottomPadding: 12
            text: "No Vulkan devices found. Install a Vulkan driver (Mesa's lavapipe works anywhere)."
            color: Theme.silverDim
            font.pixelSize: Theme.fontNormal
        }
    }

    Section {
        title: "Preferences"
        subtitle: "Written to " + graphics.sessionConfPath + ". They take effect the next time you log in."

        SettingRow {
            separator: false
            label: "Preferred device"
            description: "Automatic uses the GPU that drives the display, else the most capable one."
            NoirComboBox {
                width: 320
                accessibleName: "Preferred device"
                model: page.choices.map(c => c.length ? c : "Automatic")
                currentIndex: Math.max(0, page.choices.indexOf(page.value("preferredDevice")))
                onActivated: (i) => page.stage("preferredDevice", page.choices[i])
            }
        }
        SettingRow {
            label: "Compositor"
            description: "NoirWM blends translucent glass and draws window shadows. Off saves power and memory."
            NoirSwitch {
                accessibleName: "Compositor"
                checked: page.value("compositorEnabled")
                onToggled: (on) => page.stage("compositorEnabled", on)
            }
        }
        SettingRow {
            label: "Vulkan validation"
            description: graphics.validationAvailable ? "For development: checks every Vulkan call and logs mistakes. Slow."
                                                      : "The Khronos validation layer is not installed."
            dimmed: !graphics.validationAvailable
            NoirSwitch {
                accessibleName: "Vulkan validation"
                enabled: graphics.validationAvailable
                checked: page.value("validationEnabled")
                onToggled: (on) => page.stage("validationEnabled", on)
            }
        }
    }
}
