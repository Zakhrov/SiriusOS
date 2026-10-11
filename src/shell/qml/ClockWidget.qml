import QtQuick
import Noir.Shell

FloatingWidget {
    id: clock

    property date now: new Date()

    title: "Chronometer"
    width: 300
    height: 150

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: clock.now = new Date()
    }

    Column {
        anchors.fill: parent
        spacing: 2
        Row {
            spacing: 6
            Text {
                text: Qt.formatTime(clock.now, "hh:mm")
                color: Theme.white
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontHuge
                font.weight: Font.Light
                font.letterSpacing: 3
            }
            Text {
                anchors.baseline: parent.children[0].baseline
                text: Qt.formatTime(clock.now, "ss")
                color: Theme.silverDim
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontLarge
            }
        }
        Text {
            text: Qt.formatDate(clock.now, "dddd, d MMMM yyyy").toUpperCase()
            color: Theme.silver
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            font.letterSpacing: 2
        }
    }
}
