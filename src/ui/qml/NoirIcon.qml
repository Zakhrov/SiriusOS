import QtQuick
import Qt5Compat.GraphicalEffects
import Noir.Ui

// First-party monochrome SVG icon, tinted to the current foreground colour so
// it inverts together with its button (silver on black, black on white).
Item {
    id: icon

    property url source
    property color color: Theme.silver
    property int size: Theme.iconSize

    implicitWidth: size
    implicitHeight: size

    Image {
        id: image
        anchors.fill: parent
        source: icon.source
        sourceSize: Qt.size(icon.size * 2, icon.size * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: false
    }

    ColorOverlay {
        anchors.fill: image
        source: image
        color: icon.color
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }
}
