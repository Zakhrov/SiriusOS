import QtQuick
import Noir.Shell
import Noir.Ui

// Status indicators + notification counter + clock, bottom-right of the taskbar.
Row {
    id: tray

    property date now: new Date()
    signal notificationsClicked()

    spacing: 2
    height: Theme.taskbarHeight

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: tray.now = new Date()
    }

    NoirButton {
        anchors.verticalCenter: parent.verticalCenter
        flat: true
        width: 36; height: 36
        iconSource: Theme.icon("network")
        iconSize: 18
        opacity: SystemMonitor.networkOnline ? 1 : 0.45
        toolTip: SystemMonitor.networkOnline ? "Connected · " + SystemMonitor.networkInterface : "Offline"
        onClicked: Launcher.openControlCenter("network")
    }

    NoirButton {
        anchors.verticalCenter: parent.verticalCenter
        flat: true
        width: 36; height: 36
        iconSource: Theme.icon("volume")
        iconSize: 18
        toolTip: "Sound"
        onClicked: Launcher.openControlCenter("sound")
    }

    NoirButton {
        anchors.verticalCenter: parent.verticalCenter
        visible: SystemMonitor.batteryPresent
        flat: true
        width: 36; height: 36
        iconSource: Theme.icon("battery")
        iconSize: 18
        toolTip: "Battery " + SystemMonitor.batteryPercent + "%" + (SystemMonitor.charging ? " · charging" : "")
        onClicked: Launcher.openControlCenter("power")
    }

    NoirButton {
        id: bell
        anchors.verticalCenter: parent.verticalCenter
        flat: true
        width: 36; height: 36
        iconSource: Theme.icon("bell")
        iconSize: 18
        opacity: NoirConfig.doNotDisturb ? 0.45 : 1
        toolTip: NoirConfig.doNotDisturb ? "Do not disturb" + (NotificationServer.count ? " · clear " + NotificationServer.count : "")
               : NotificationServer.count ? "Clear " + NotificationServer.count + " notification" + (NotificationServer.count === 1 ? "" : "s") : "No notifications"
        onClicked: tray.notificationsClicked()

        Rectangle {
            visible: NotificationServer.count > 0
            anchors { right: parent.right; top: parent.top; margins: 3 }
            width: 14; height: 14
            color: bell.inverted ? Theme.black : Theme.white
            Text {
                anchors.centerIn: parent
                text: NotificationServer.count
                color: bell.inverted ? Theme.white : Theme.black
                font.pixelSize: 9
                font.bold: true
            }
        }
    }

    Rectangle { width: 1; height: 26; color: Theme.silverFaint; anchors.verticalCenter: parent.verticalCenter }

    NoirButton {
        id: clockButton
        anchors.verticalCenter: parent.verticalCenter
        flat: true
        width: 92; height: Theme.taskbarHeight - 10
        toolTip: Qt.formatDate(tray.now, "dddd, d MMMM yyyy")
        onClicked: Launcher.openControlCenter("datetime")
        Accessible.name: Qt.formatTime(tray.now, NoirConfig.timeFormat(false)) + ", " + Qt.formatDate(tray.now, "dddd d MMMM")

        Column {
            anchors.centerIn: parent
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Qt.formatTime(tray.now, NoirConfig.timeFormat(false))
                color: clockButton.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal + 1
                font.letterSpacing: 1.5
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: NoirConfig.clockShowDate
                text: Qt.formatDate(tray.now, "dd.MM.yyyy")
                color: clockButton.inverted ? Theme.black : Theme.silverDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall - 1
                font.letterSpacing: 1
            }
        }
    }
}
