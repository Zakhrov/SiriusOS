import QtQuick
import Noir.Ui

// The light catching a pane of glass: a soft diagonal band across its upper
// left. Fill the glass with it, above its tint and below its content.
Item {
    id: reflection

    property real strength: 1.0

    clip: true

    Rectangle {
        // A band twice as long as the surface's diagonal, tilted, so it
        // crosses the corner whatever the surface's proportions.
        readonly property real span: Math.sqrt(reflection.width * reflection.width + reflection.height * reflection.height)
        width: span * 2
        height: Math.max(reflection.height * 0.9, 60)
        x: -span * 0.6
        y: -height * 0.55
        rotation: -18
        transformOrigin: Item.Center
        opacity: reflection.strength
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.55; color: Theme.glassReflection }
            GradientStop { position: 0.62; color: Qt.rgba(1, 1, 1, Theme.glassReflection.a * 0.35) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
}
