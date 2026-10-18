import QtQuick
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    property string server: ControlCenterService.notificationServer()

    staged: true
    defaults: ({ doNotDisturb: false, notificationTimeout: 6 })
    function current(key) { return NoirConfig[key] }
    function commit(key, value) { NoirConfig[key] = value }

    Banner { id: banner }

    Section {
        title: "Pop-ups"

        SettingRow {
            separator: false
            label: "Do not disturb"
            description: "Keep notifications off the screen. They are still counted on the bell in the taskbar."
            NoirSwitch {
                accessibleName: "Do not disturb"
                checked: page.value("doNotDisturb")
                onToggled: (on) => page.stage("doNotDisturb", on)
            }
        }
        SettingRow {
            label: "Show notifications for"
            description: "Unless the application asks for another time. Critical notifications stay until dismissed."
            Row {
                spacing: 12
                NoirSlider {
                    id: timeout
                    width: 240
                    accessibleName: "Show notifications for"
                    from: 2; to: 30; stepSize: 1
                    value: page.value("notificationTimeout")
                    onMoved: (v) => page.stage("notificationTimeout", Math.round(v))
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 60
                    text: Math.round(timeout.current) + " s"
                    color: Theme.silver
                    font.pixelSize: Theme.fontNormal
                }
            }
        }
        SettingRow {
            label: "Try it"
            description: page.server.length ? "Notifications are shown by " + page.server + ". Apply first to try new settings."
                                            : "No notification service is running in this session."
            NoirButton {
                text: "Send a test notification"
                iconSource: Theme.icon("bell")
                enabled: page.server.length > 0
                onClicked: {
                    if (!ControlCenterService.sendTestNotification())
                        banner.show("The notification service did not answer.")
                    else if (NoirConfig.doNotDisturb)
                        banner.show("Sent. Do not disturb is on, so it only shows up on the taskbar's bell.", "info")
                }
            }
        }
    }
}
