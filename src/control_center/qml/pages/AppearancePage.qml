import QtQuick
import QtQuick.Effects
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    // Every key is a NoirConfig property of the same name.
    staged: true
    defaults: ({
        reducedMotion: false, animationScale: 1.0, glassOpacity: 0.6, glassBlur: true,
        showDesktopIcons: true, showDesktopWidgets: true, showWallpaperGrid: true,
    })
    function current(key) { return NoirConfig[key] }
    function commit(key, value) { NoirConfig[key] = value }

    Section {
        title: "Motion"
        subtitle: "Every 3D effect in Noir follows these settings, in the shell and in the Control Center."

        SettingRow {
            separator: false
            label: "Reduce motion"
            description: "Turn animations, fold-outs and floating widgets into instant changes."
            NoirSwitch {
                accessibleName: "Reduce motion"
                checked: page.value("reducedMotion")
                onToggled: (on) => page.stage("reducedMotion", on)
            }
        }
        SettingRow {
            label: "Animation duration"
            description: "Shorter is snappier; longer shows the 3D motion more clearly."
            dimmed: page.value("reducedMotion")
            Row {
                spacing: 12
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Fast"
                    color: Theme.silverDim
                    font.pixelSize: Theme.fontSmall
                }
                NoirSlider {
                    id: speed
                    width: 220
                    accessibleName: "Animation duration"
                    enabled: !page.value("reducedMotion")
                    from: 0.5; to: 2.0; stepSize: 0.1
                    value: page.value("animationScale")
                    onMoved: (v) => page.stage("animationScale", Math.round(v * 10) / 10)
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 76
                    text: "Slow · " + Math.round(speed.current * 100) + "%"
                    color: Theme.silverDim
                    font.pixelSize: Theme.fontSmall
                }
            }
        }
    }

    Section {
        title: "Glass"
        subtitle: "Panels, the taskbar, menus, notifications and widgets are black glass. Seeing through it needs NoirWM's compositor (see Graphics)."

        SettingRow {
            separator: false
            label: "Glass opacity"
            description: Math.round(page.value("glassOpacity") * 100) + "% — lower shows more of what is behind, 100% is solid black."
            NoirSlider {
                width: 300
                accessibleName: "Glass opacity"
                from: 0.3; to: 1.0; stepSize: 0.02
                value: page.value("glassOpacity")
                onMoved: (v) => page.stage("glassOpacity", Math.round(v * 100) / 100)
            }
        }
        SettingRow {
            label: "Blur behind glass"
            description: "Frosted glass: what shows through is blurred, so text on the glass stays legible."
            NoirSwitch {
                accessibleName: "Blur behind glass"
                checked: page.value("glassBlur")
                onToggled: (on) => page.stage("glassBlur", on)
            }
        }

        // Preview of the staged settings: a pane of glass over a busy backdrop.
        Item {
            width: parent.width
            height: 150
            Item {
                id: backdrop
                anchors { fill: parent; topMargin: 8; bottomMargin: 12 }
                clip: true
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#000000" }
                        GradientStop { position: 1; color: "#1a1c22" }
                    }
                }
                Repeater {
                    model: 24
                    Rectangle {
                        required property int index
                        x: index * backdrop.width / 12 - backdrop.width / 2 + (index - 12) * 4
                        width: 2
                        height: backdrop.height * 1.4
                        rotation: (index - 12) * 4
                        color: Qt.rgba(0.76, 0.78, 0.82, 0.35)
                    }
                }
                Text {
                    anchors.centerIn: parent
                    text: "S I R I U S   O S"
                    color: Theme.white
                    font.pixelSize: 34
                    font.weight: Font.Light
                    font.letterSpacing: 6
                }
            }
            ShaderEffectSource {
                id: behindPane
                sourceItem: backdrop
                sourceRect: Qt.rect(pane.x - backdrop.x, pane.y - backdrop.y, pane.width, pane.height)
                visible: false
            }
            Item {
                id: pane
                anchors.centerIn: backdrop
                width: 340; height: 74
                MultiEffect {
                    anchors.fill: parent
                    visible: page.value("glassBlur")
                    source: behindPane
                    autoPaddingEnabled: false
                    blurEnabled: true
                    blur: 1.0
                    blurMax: 40
                    saturation: -0.4
                }
                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(0.03, 0.035, 0.045, page.value("glassOpacity"))
                    border.width: 1
                    border.color: Theme.glassEdge
                }
                GlassReflection { anchors.fill: parent; anchors.margins: 1 }
                Row {
                    anchors.centerIn: parent
                    spacing: 10
                    NoirIcon { source: Theme.icon("appearance"); size: 22; color: Theme.white; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Black glass, " + Math.round(page.value("glassOpacity") * 100) + "%"
                              + (page.value("glassBlur") ? ", frosted" : "")
                        color: Theme.white
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        font.letterSpacing: Theme.letterSpacing
                    }
                }
            }
        }
    }

    Section {
        title: "Desktop"

        SettingRow {
            separator: false
            label: "Desktop icons"
            description: "Home, Terminal, Settings and Trash on the left."
            NoirSwitch {
                accessibleName: "Desktop icons"
                checked: page.value("showDesktopIcons")
                onToggled: (on) => page.stage("showDesktopIcons", on)
            }
        }
        SettingRow {
            label: "Desktop widgets"
            description: "The floating chronometer and system monitor."
            NoirSwitch {
                accessibleName: "Desktop widgets"
                checked: page.value("showDesktopWidgets")
                onToggled: (on) => page.stage("showDesktopWidgets", on)
            }
        }
        SettingRow {
            label: "Perspective grid"
            description: "The receding silver grid on the wallpaper."
            NoirSwitch {
                accessibleName: "Perspective grid"
                checked: page.value("showWallpaperGrid")
                onToggled: (on) => page.stage("showWallpaperGrid", on)
            }
        }
    }
}
