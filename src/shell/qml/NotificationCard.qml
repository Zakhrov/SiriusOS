import QtQuick
import Noir.Shell

// One notification: glass card with app line, summary, body, optional
// progress and action buttons. Click (or Enter) dismisses; Delete closes.
FocusScope {
    id: card

    property int notificationId
    property string appName
    property string summary
    property string body
    property string iconName
    property int urgency: 1
    property var actions: []
    property int progress: -1
    property real swing: 0      // Y-axis tilt used by the slide in/out transitions

    transform: Rotation {
        origin.x: card.width
        origin.y: card.height / 2
        axis { x: 0; y: 1; z: 0 }
        angle: card.swing
    }

    width: 360
    height: content.implicitHeight + 28
    activeFocusOnTab: true
    Accessible.role: Accessible.AlertMessage
    Accessible.name: summary
    Accessible.description: body

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Delete || event.key === Qt.Key_Escape || event.key === Qt.Key_Return) {
            NotificationServer.dismiss(card.notificationId)
            event.accepted = true
        }
    }

    GlassPanel {
        anchors.fill: parent
        color: Theme.glassStrong
        edgeColor: card.urgency >= 2 || card.activeFocus ? Theme.white : Theme.glassEdge
        edgeWidth: card.urgency >= 2 ? 2 : 1
        elevation: 8
    }
    // Critical notifications get a solid white spine.
    Rectangle {
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: card.urgency >= 2 ? 4 : 2
        color: card.urgency >= 2 ? Theme.white : Theme.silverDim
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: NotificationServer.dismiss(card.notificationId)
    }

    Row {
        id: content
        x: 16; y: 14
        width: card.width - 32
        spacing: 12

        Item {
            width: 36; height: 36
            Rectangle { anchors.fill: parent; color: Theme.black; border.color: Theme.silverFaint }
            Image {
                id: icon
                anchors.centerIn: parent
                width: 28; height: 28
                sourceSize: Qt.size(56, 56)
                source: card.iconName.length ? "image://appicon/" + encodeURIComponent(card.iconName) : ""
                visible: status === Image.Ready && implicitWidth > 1
            }
            NoirIcon {
                anchors.centerIn: parent
                visible: !icon.visible
                size: 20
                source: "icons/noir-bell.svg"
            }
        }

        Column {
            width: content.width - 48
            spacing: 4

            Text {
                width: parent.width
                text: card.appName.toUpperCase()
                color: Theme.silverDim
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall - 1
                font.letterSpacing: 2
            }
            Text {
                width: parent.width
                text: card.summary
                color: Theme.white
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal + 1
                font.bold: true
            }
            Text {
                width: parent.width
                visible: text.length > 0
                text: card.body
                textFormat: Text.PlainText
                color: Theme.silver
                wrapMode: Text.Wrap
                maximumLineCount: 4
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
            }
            ProgressBar3D {
                visible: card.progress >= 0
                width: parent.width
                value: card.progress / 100
            }
            Row {
                visible: card.actions.length > 0
                spacing: 6
                topPadding: 4
                Repeater {
                    model: card.actions
                    NoirButton {
                        required property var modelData
                        text: modelData.label
                        height: 28
                        onClicked: NotificationServer.invokeAction(card.notificationId, modelData.key)
                    }
                }
            }
        }
    }

    // Close glyph, top right.
    NoirButton {
        anchors { right: parent.right; top: parent.top; margins: 6 }
        width: 22; height: 22
        flat: true
        visible: mouse.containsMouse || hovered || card.activeFocus
        text: "×"
        horizontalPadding: 0
        Accessible.name: "Dismiss"
        onClicked: NotificationServer.dismiss(card.notificationId)
    }
}
