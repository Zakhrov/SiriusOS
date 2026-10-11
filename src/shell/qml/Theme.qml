pragma Singleton
import QtQuick

// Noir design tokens, straight from VDD.md: black with silver/white highlights,
// glass transparency, sharp (zero-radius) edges, and smooth 3D motion.
QtObject {
    id: theme

    // --- Palette -----------------------------------------------------------
    readonly property color black: "#050506"
    readonly property color abyss: "#000000"
    readonly property color panel: "#121317"
    readonly property color silver: "#c3c7cf"
    readonly property color silverDim: "#6c7079"
    readonly property color silverFaint: "#2b2d33"
    readonly property color white: "#f4f6fa"

    // Glass: translucent black with a silver edge and a faint top sheen.
    readonly property color glass: Qt.rgba(0.03, 0.035, 0.045, 0.72)
    readonly property color glassStrong: Qt.rgba(0.02, 0.022, 0.03, 0.88)
    readonly property color glassEdge: Qt.rgba(0.76, 0.78, 0.82, 0.55)
    readonly property color glassSheen: Qt.rgba(1, 1, 1, 0.07)
    readonly property color shadow: Qt.rgba(0, 0, 0, 0.55)

    // Interactive elements invert on hover / keyboard focus (accessibility).
    readonly property color invertedBackground: white
    readonly property color invertedForeground: black

    // --- Geometry ----------------------------------------------------------
    readonly property int radius: 0            // sharp edges everywhere
    readonly property int edge: 1              // hairline silver edges
    readonly property int chamfer: 14          // polygon cut for widgets
    readonly property int taskbarHeight: 48
    readonly property int spacing: 8
    readonly property int iconSize: 24
    readonly property int desktopIconSize: 64

    // --- Typography --------------------------------------------------------
    readonly property string fontFamily: Qt.application.font.family
    readonly property string monoFamily: "monospace"
    readonly property int fontSmall: 11
    readonly property int fontNormal: 13
    readonly property int fontLarge: 18
    readonly property int fontHuge: 44
    readonly property real letterSpacing: 0.8

    // --- Motion ------------------------------------------------------------
    // Set reducedMotion to collapse every animation to an instant change.
    property bool reducedMotion: false
    readonly property int durationFast: reducedMotion ? 0 : 110
    readonly property int durationNormal: reducedMotion ? 0 : 220
    readonly property int durationSlow: reducedMotion ? 0 : 420
    readonly property int floatPeriod: 3600
    readonly property real pressDepth: 0.95       // 3D press: scale when pushed
    readonly property real perspectiveTilt: 68    // degrees for menu fold-outs
}
