import QtQuick
import Noir.Shell

// Status indicators + notification counter + clock, bottom-right of the taskbar.
Row {
    id: tray

    property date now: new Date()
    signal clockClicked()
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
        iconSource: "icons/noir-network.svg"
        iconSize: 18
        opacity: SystemMonitor.networkOnline ? 1 : 0.45
        toolTip: SystemMonitor.networkOnline ? "Connected · " + SystemMonitor.networkInterface : "Offline"
    }

    NoirButton {
        anchors.verticalCenter: parent.verticalCenter
        flat: true
        width: 36; height: 36
        iconSource: "icons/noir-volume.svg"
        iconSize: 18
        toolTip: "Volume"
    }

    NoirButton {
        anchors.verticalCenter: parent.verticalCenter
        visible: SystemMonitor.batteryPresent
        flat: true
        width: 36; height: 36
        iconSource: "icons/noir-battery.svg"
        iconSize: 18
        toolTip: "Battery " + SystemMonitor.batteryPercent + "%" + (SystemMonitor.charging ? " · charging" : "")
    }

    NoirButton {
        id: bell
        anchors.verticalCenter: parent.verticalCenter
        flat: true
        width: 36; height: 36
        iconSource: "icons/noir-bell.svg"
        iconSize: 18
        toolTip: NotificationServer.count ? "Clear " + NotificationServer.count + " notification" + (NotificationServer.count === 1 ? "" : "s") : "No notifications"
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
        onClicked: tray.clockClicked()
        Accessible.name: Qt.formatDateTime(tray.now, "hh:mm, dddd d MMMM")

        Column {
            anchors.centerIn: parent
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Qt.formatTime(tray.now, "hh:mm")
                color: clockButton.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal + 1
                font.letterSpacing: 1.5
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Qt.formatDate(tray.now, "dd.MM.yyyy")
                color: clockButton.inverted ? Theme.black : Theme.silverDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall - 1
                font.letterSpacing: 1
            }
        }
    }
}
