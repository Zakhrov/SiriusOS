import QtQuick
import Noir.Shell

// Sharp-edged black glass surface: translucent fill, silver hairline edge,
// a faint top sheen and an optional layered drop shadow.
Item {
    id: panel

    property color color: Theme.glass
    property color edgeColor: Theme.glassEdge
    property int edgeWidth: Theme.edge
    property bool sheen: true
    property int elevation: 0      // 0 = flat, larger = softer, deeper shadow

    default property alias content: contentItem.data

    Repeater {
        model: panel.elevation > 0 ? 4 : 0
        Rectangle {
            readonly property real spread: (index + 1) * panel.elevation / 4
            x: spread * 0.4 - spread / 2 + panel.elevation / 3
            y: spread * 0.4 - spread / 2 + panel.elevation / 2
            width: panel.width + spread
            height: panel.height + spread
            color: Theme.abyss
            opacity: 0.14
        }
    }

    Rectangle {
        anchors.fill: parent
        color: panel.color
        border.color: panel.edgeColor
        border.width: panel.edgeWidth
        radius: Theme.radius
    }

    Rectangle {
        visible: panel.sheen
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: panel.edgeWidth }
        height: Math.min(parent.height / 2, 28)
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.glassSheen }
            GradientStop { position: 1; color: "transparent" }
        }
    }

    Item {
        id: contentItem
        anchors.fill: parent
    }
}
