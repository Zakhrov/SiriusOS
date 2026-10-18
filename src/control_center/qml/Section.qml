import QtQuick
import Noir.Ui

// A titled group of settings on a black glass panel, in the style of the
// desktop widgets: a silver tick and a spaced-out upper-case title.
Item {
    id: section

    property string title
    property string subtitle
    // Extra items on the right of the title (buttons, status).
    property alias actions: actionRow.data
    default property alias content: body.data

    width: parent ? parent.width : 0
    implicitHeight: body.y + body.implicitHeight + 14
    height: implicitHeight

    GlassPanel {
        anchors.fill: parent
        color: Theme.glass
    }

    Item {
        id: head
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 18; rightMargin: 14; topMargin: 14 }
        height: Math.max(titleColumn.implicitHeight, actionRow.implicitHeight)

        Rectangle {
            anchors { left: parent.left; top: parent.top; topMargin: 2 }
            width: 3; height: 12
            color: Theme.white
        }
        Column {
            id: titleColumn
            anchors { left: parent.left; leftMargin: 12; right: actionRow.left; rightMargin: 12; top: parent.top }
            spacing: 4
            Text {
                text: section.title.toUpperCase()
                color: Theme.silver
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                font.letterSpacing: 2.5
            }
            Text {
                visible: section.subtitle.length > 0
                width: parent.width
                text: section.subtitle
                color: Theme.silverDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }
        }
        Row {
            id: actionRow
            anchors { right: parent.right; verticalCenter: titleColumn.verticalCenter }
            spacing: 8
        }
    }

    Column {
        id: body
        anchors { left: parent.left; right: parent.right; top: head.bottom; leftMargin: 18; rightMargin: 18; topMargin: 10 }
        spacing: 0
    }
}
