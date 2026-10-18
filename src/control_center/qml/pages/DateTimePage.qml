import QtQuick
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    property date now: new Date()
    readonly property var zones: {
        const words = zoneFilter.text.toLowerCase().split(/\s+/).filter(w => w.length)
        const list = dateTime.timezones
        return words.length ? list.filter(z => words.every(w => z.toLowerCase().replace(/_/g, " ").indexOf(w) >= 0)) : list
    }

    // Setting the clock by hand is an action (the Set button); the rest waits for Apply.
    staged: true
    defaults: ({ clock24Hour: true, clockShowSeconds: true, clockShowDate: true })
    function current(key) {
        if (key === "timezone" || key === "ntp")
            return dateTime[key]
        return NoirConfig[key]
    }
    function commit(key, value) {
        if (key === "timezone") dateTime.setTimezone(value)
        else if (key === "ntp") dateTime.setNtp(value)
        else NoirConfig[key] = value
    }

    DateTimeConfig {
        id: dateTime
        onErrorOccurred: (message) => banner.show(message)
    }

    Timer {
        interval: 1000
        repeat: true
        running: page.active
        triggeredOnStart: true
        onTriggered: page.now = new Date()
    }

    Banner { id: banner }

    Section {
        title: "Clock"

        Item {
            width: parent.width
            height: 96
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text {
                    text: Qt.formatTime(page.now, NoirConfig.timeFormat(true))
                    color: Theme.white
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontHuge
                    font.weight: Font.Light
                    font.letterSpacing: 3
                }
                Text {
                    text: Qt.formatDate(page.now, "dddd, d MMMM yyyy").toUpperCase() + "  ·  " + dateTime.timezone
                    color: Theme.silver
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.letterSpacing: 2
                }
            }
        }

        SettingRow {
            label: "Set the time automatically"
            description: !dateTime.available ? "The time service (systemd-timedated) is not available."
                       : !dateTime.canNtp ? "No network time service is installed."
                       : dateTime.ntp ? (dateTime.ntpSynchronized ? "Synchronised with network time servers." : "Waiting to synchronise…")
                       : "Off: set the clock below."
            NoirSwitch {
                accessibleName: "Set the time automatically"
                enabled: dateTime.available && dateTime.canNtp && !dateTime.busy
                checked: page.value("ntp")
                onToggled: (on) => page.stage("ntp", on)
            }
        }
        SettingRow {
            label: "Date and time"
            description: dateTime.ntp ? "Turn off automatic time, and apply, to set the clock yourself."
                                      : "Year-month-day and 24-hour time. Sets the clock at once."
            dimmed: dateTime.ntp
            Row {
                spacing: 8
                NoirTextField {
                    id: dateField
                    width: 130
                    accessibleName: "Date"
                    enabled: !dateTime.ntp && dateTime.available
                    placeholderText: "YYYY-MM-DD"
                    text: Qt.formatDate(new Date(), "yyyy-MM-dd")
                }
                NoirTextField {
                    id: timeField
                    width: 110
                    accessibleName: "Time"
                    enabled: !dateTime.ntp && dateTime.available
                    placeholderText: "HH:MM:SS"
                    text: Qt.formatTime(new Date(), "HH:mm:ss")
                }
                NoirButton {
                    text: "Set"
                    enabled: !dateTime.ntp && dateTime.available && !dateTime.busy
                    onClicked: {
                        const d = new Date(dateField.text + "T" + timeField.text)
                        if (isNaN(d.getTime()))
                            banner.show("That is not a valid date and time.")
                        else
                            dateTime.setDateTime(d)
                    }
                }
            }
        }
    }

    Section {
        title: "Time zone"
        subtitle: page.value("timezone") === dateTime.timezone ? "Current: " + dateTime.timezone
                  : "Current: " + dateTime.timezone + " — " + page.value("timezone") + " once applied"

        NoirTextField {
            id: zoneFilter
            width: parent.width
            placeholderText: "Search time zones (city or region)"
            iconSource: Theme.icon("search")
            accessibleName: "Search time zones"
            Keys.onDownPressed: zoneList.forceActiveFocus()
            onAccepted: if (page.zones.length) page.stage("timezone", page.zones[0])
        }
        Item { width: 1; height: 8 }
        Item {
            width: parent.width
            height: 260

            Rectangle { anchors.fill: parent; color: Theme.abyss; border.width: 1; border.color: Theme.silverFaint }
            ListView {
                id: zoneList
                anchors { fill: parent; margins: 1; rightMargin: zoneBar.visible ? 12 : 1 }
                clip: true
                model: page.zones
                activeFocusOnTab: true
                boundsBehavior: Flickable.StopAtBounds
                currentIndex: page.zones.indexOf(page.value("timezone"))
                Component.onCompleted: positionViewAtIndex(Math.max(0, currentIndex), ListView.Center)
                Keys.onReturnPressed: page.stage("timezone", page.zones[currentIndex])
                Keys.onEnterPressed: page.stage("timezone", page.zones[currentIndex])
                delegate: Item {
                    id: zone
                    required property string modelData
                    required property int index
                    readonly property bool current: modelData === page.value("timezone")
                    readonly property bool highlighted: current || (zoneList.activeFocus && ListView.isCurrentItem) || zoneMouse.containsMouse
                    width: ListView.view.width
                    height: 28
                    Rectangle {
                        anchors.fill: parent
                        color: zone.current ? Theme.white : (zone.highlighted ? Qt.rgba(1, 1, 1, 0.12) : "transparent")
                    }
                    Text {
                        anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                        text: zone.modelData.replace(/_/g, " ")
                        color: zone.current ? Theme.black : Theme.silver
                        font.pixelSize: Theme.fontNormal
                    }
                    MouseArea {
                        id: zoneMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: { zoneList.currentIndex = zone.index; page.stage("timezone", zone.modelData) }
                    }
                }
            }
            NoirScrollBar {
                id: zoneBar
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom; margins: 2 }
                flickable: zoneList
            }
        }
        Item { width: 1; height: 6 }
    }

    Section {
        title: "Clock format"
        subtitle: "For the taskbar and the desktop's chronometer."

        SettingRow {
            separator: false
            label: "24-hour clock"
            description: "Now: " + Qt.formatTime(page.now, page.value("clock24Hour") ? "hh:mm" : "h:mm AP")
            NoirSwitch {
                accessibleName: "24-hour clock"
                checked: page.value("clock24Hour")
                onToggled: (on) => page.stage("clock24Hour", on)
            }
        }
        SettingRow {
            label: "Seconds on the chronometer"
            NoirSwitch {
                accessibleName: "Show seconds"
                checked: page.value("clockShowSeconds")
                onToggled: (on) => page.stage("clockShowSeconds", on)
            }
        }
        SettingRow {
            label: "Date in the taskbar"
            NoirSwitch {
                accessibleName: "Show the date in the taskbar"
                checked: page.value("clockShowDate")
                onToggled: (on) => page.stage("clockShowDate", on)
            }
        }
    }
}
