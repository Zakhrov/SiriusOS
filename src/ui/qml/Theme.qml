pragma Singleton
import QtQuick
import Noir.Ui

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

    // Glass: translucent black with a silver edge and a faint top sheen. The
    // opacity is a user setting (Control Center > Appearance).
    readonly property real glassOpacity: Math.max(0.3, Math.min(1, NoirConfig.glassOpacity))
    readonly property color glass: Qt.rgba(0.03, 0.035, 0.045, glassOpacity)
    // Menus and cards: a little denser for legibility; frosted glass needs less.
    readonly property color glassStrong: Qt.rgba(0.02, 0.022, 0.03, Math.min(1, glassOpacity + (glassBlur ? 0.06 : 0.16)))
    readonly property color glassEdge: Qt.rgba(0.76, 0.78, 0.82, 0.55)
    readonly property color glassSheen: Qt.rgba(1, 1, 1, 0.07)
    // Large glass surfaces (taskbar, widgets) are lit from above: a lighter,
    // clearer top fading into a darker, denser bottom.
    readonly property color glassTop: Qt.rgba(0.10, 0.105, 0.12, glassOpacity * 0.55)
    readonly property color glassBottom: Qt.rgba(0.0, 0.0, 0.01, Math.min(1, glassOpacity * 0.95))
    // The diagonal reflection that makes a surface read as glass, even over black.
    readonly property color glassReflection: Qt.rgba(1, 1, 1, 0.085)
    // Frosted glass: blur what lies behind glass surfaces (a user setting).
    readonly property bool glassBlur: NoirConfig.glassBlur
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
    // Reduced motion collapses every animation to an instant change; the
    // scale stretches (> 1) or speeds up (< 1) all of them. Both are user
    // settings (Control Center > Appearance).
    readonly property bool reducedMotion: NoirConfig.reducedMotion
    readonly property real animationScale: Math.max(0.25, Math.min(3, NoirConfig.animationScale))
    readonly property int durationFast: reducedMotion ? 0 : Math.round(110 * animationScale)
    readonly property int durationNormal: reducedMotion ? 0 : Math.round(220 * animationScale)
    readonly property int durationSlow: reducedMotion ? 0 : Math.round(420 * animationScale)
    readonly property int floatPeriod: 3600
    readonly property real pressDepth: 0.95       // 3D press: scale when pushed
    readonly property real perspectiveTilt: 68    // degrees for menu fold-outs

    // --- Icons -------------------------------------------------------------
    // First-party icon by name: icon("settings") is icons/noir-settings.svg.
    function icon(name) {
        return "qrc:/Noir/Ui/icons/noir-" + name + ".svg"
    }
}
