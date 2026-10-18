import QtQuick
import Noir.Ui

// Message strip at the top of a page. Errors are inverted (white on black's
// opposite) so they stand out in a black-and-silver palette; information is
// glass with a silver edge and goes away by itself. Optional action buttons
// sit on the right.
Item {
    id: banner

    property string text
    property string kind: "error"          // "error" | "info"
    property bool shown: false
    property var actions: []               // [{ text, action }]
    property int autoHideMs: 6000
    readonly property bool inverted: kind === "error"

    function show(message, messageKind, messageActions) {
        text = message
        kind = messageKind || "error"
        actions = messageActions || []
        shown = true
        if (kind === "info" && actions.length === 0)
            hideTimer.restart()
        else
            hideTimer.stop()
    }
    function hide() {
        shown = false
        hideTimer.stop()
    }

    width: parent ? parent.width : 0
    height: shown ? box.height : 0
    visible: height > 0
    clip: true
    Behavior on height { NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic } }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: text

    Timer { id: hideTimer; interval: banner.autoHideMs; onTriggered: banner.hide() }

    Rectangle {
        id: box
        width: parent.width
        height: Math.max(48, message.implicitHeight + 24)
        color: banner.inverted ? Theme.white : Theme.glassStrong
        border.width: Theme.edge
        border.color: banner.inverted ? Theme.white : Theme.silver

        NoirIcon {
            id: glyph
            anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
            source: Theme.icon(banner.inverted ? "close" : "info")
            size: 16
            color: banner.inverted ? Theme.black : Theme.silver
        }
        Text {
            id: message
            anchors { left: glyph.right; right: buttons.left; leftMargin: 12; rightMargin: 12; verticalCenter: parent.verticalCenter }
            text: banner.text
            color: banner.inverted ? Theme.black : Theme.white
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            wrapMode: Text.WordWrap
        }
        Row {
            id: buttons
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            spacing: 6
            Repeater {
                model: banner.actions
                NoirButton {
                    required property var modelData
                    text: modelData.text
                    onClicked: modelData.action()
                }
            }
            // Dismiss: drawn in the banner's own colours, inverting on hover.
            Rectangle {
                id: dismissButton
                readonly property bool lit: dismiss.containsMouse || activeFocus
                visible: banner.actions.length === 0
                width: 30; height: 30
                activeFocusOnTab: true
                color: lit ? (banner.inverted ? Theme.black : Theme.white) : "transparent"
                Keys.onSpacePressed: banner.hide()
                Keys.onReturnPressed: banner.hide()
                Accessible.role: Accessible.Button
                Accessible.name: "Dismiss"
                Accessible.onPressAction: banner.hide()
                NoirIcon {
                    anchors.centerIn: parent
                    source: Theme.icon("close")
                    size: 12
                    color: (banner.inverted !== dismissButton.lit) ? Theme.black : Theme.white
                }
                MouseArea {
                    id: dismiss
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: banner.hide()
                }
            }
        }
    }
}
