import QtQuick
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    readonly property var delays: [0, 1, 2, 5, 10, 15, 20, 30, 45, 60]
    readonly property var profileNames: ({ "power-saver": "Power saver", "balanced": "Balanced", "performance": "Performance" })

    function delayText(minutes) {
        return minutes === 0 ? "Never" : minutes === 1 ? "1 minute" : minutes + " minutes"
    }
    function confirm(action, label) {
        banner.show(label + " now?", "info", [
            { text: label, action: () => { banner.hide(); power.sessionAction(action) } },
            { text: "Cancel", action: () => banner.hide() },
        ])
    }

    // Session actions (suspend, restart, ...) act at once; settings wait for Apply.
    staged: true
    defaults: power.profilesAvailable && power.profiles.indexOf("balanced") >= 0
              ? ({ activeProfile: "balanced", blankMinutes: 10, screenOffMinutes: 15 })
              : ({ blankMinutes: 10, screenOffMinutes: 15 })
    function current(key) { return power[key] }
    function commit(key, value) {
        if (key === "activeProfile") power.setActiveProfile(value)
        else if (key === "blankMinutes") power.setBlankMinutes(value)
        else if (key === "screenOffMinutes") power.setScreenOffMinutes(value)
    }

    PowerConfig {
        id: power
        onErrorOccurred: (message) => banner.show(message)
    }

    Timer {
        interval: 10000
        repeat: true
        running: page.active
        onTriggered: power.refresh()
    }

    Banner { id: banner }

    Section {
        title: "Battery"
        subtitle: !power.upowerAvailable ? "The power service (UPower) is not running."
                : power.batteries.length === 0 ? "No battery: this machine runs on mains power." : ""

        Repeater {
            model: power.batteries
            Column {
                id: battery
                required property var modelData
                required property int index
                width: parent.width
                spacing: 0
                topPadding: index > 0 ? 10 : 0

                Item {
                    width: parent.width
                    height: 54
                    Text {
                        anchors { left: parent.left; top: parent.top; topMargin: 6 }
                        text: battery.modelData.name
                        color: Theme.white
                        font.pixelSize: Theme.fontNormal
                    }
                    Text {
                        anchors { right: parent.right; top: parent.top; topMargin: 6 }
                        text: battery.modelData.state
                              + (battery.modelData.timeToEmpty ? " · " + battery.modelData.timeToEmpty + " left" : "")
                              + (battery.modelData.timeToFull ? " · full in " + battery.modelData.timeToFull : "")
                        color: Theme.silver
                        font.pixelSize: Theme.fontSmall
                    }
                    ProgressBar3D {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 8 }
                        height: 14
                        value: battery.modelData.percentage
                    }
                }
                InfoRow { label: "Power draw"; value: battery.modelData.rate }
                InfoRow {
                    label: "Health"
                    value: battery.modelData.health >= 0 ? Math.round(battery.modelData.health * 100) + "% — " + battery.modelData.capacity : ""
                }
                InfoRow { label: "Chemistry"; value: battery.modelData.technology }
            }
        }
    }

    Section {
        visible: power.profilesAvailable
        title: "Performance"

        SettingRow {
            separator: false
            label: "Power profile"
            description: "Balances speed against heat, noise and battery life."
            NoirComboBox {
                width: 240
                accessibleName: "Power profile"
                model: power.profiles.map(p => page.profileNames[p] || p)
                currentIndex: power.profiles.indexOf(page.value("activeProfile"))
                onActivated: (i) => page.stage("activeProfile", power.profiles[i])
            }
        }
    }

    Section {
        title: "Screen"
        subtitle: "Saved, and applied again each time you log in."

        SettingRow {
            separator: false
            label: "Blank the screen after"
            description: "Without input for this long, the screen goes black."
            NoirComboBox {
                width: 200
                accessibleName: "Blank the screen after"
                model: page.delays.map(page.delayText)
                currentIndex: page.delays.indexOf(page.value("blankMinutes"))
                onActivated: (i) => page.stage("blankMinutes", page.delays[i])
            }
        }
        SettingRow {
            label: "Turn the screen off after"
            description: power.dpmsAvailable ? "The monitor goes into power saving (DPMS)."
                                             : "This display cannot be powered down by the X server (no DPMS)."
            dimmed: !power.dpmsAvailable
            NoirComboBox {
                width: 200
                accessibleName: "Turn the screen off after"
                enabled: power.dpmsAvailable
                model: page.delays.map(page.delayText)
                currentIndex: page.delays.indexOf(page.value("screenOffMinutes"))
                onActivated: (i) => page.stage("screenOffMinutes", page.delays[i])
            }
        }
    }

    Section {
        title: "Session"

        Flow {
            width: parent.width
            spacing: 8
            topPadding: 4
            bottomPadding: 10
            NoirButton {
                text: "Suspend"
                iconSource: Theme.icon("power")
                iconSize: 14
                enabled: !!power.sessionActions.canSuspend
                onClicked: power.sessionAction("Suspend")
            }
            NoirButton {
                text: "Hibernate"
                iconSource: Theme.icon("power")
                iconSize: 14
                enabled: !!power.sessionActions.canHibernate
                onClicked: page.confirm("Hibernate", "Hibernate")
            }
            NoirButton {
                text: "Restart…"
                iconSource: Theme.icon("refresh")
                iconSize: 14
                enabled: !!power.sessionActions.canReboot
                onClicked: page.confirm("Reboot", "Restart")
            }
            NoirButton {
                text: "Shut Down…"
                iconSource: Theme.icon("power")
                iconSize: 14
                enabled: !!power.sessionActions.canPowerOff
                onClicked: page.confirm("PowerOff", "Shut down")
            }
        }
    }
}
