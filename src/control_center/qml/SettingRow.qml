import QtQuick
import Noir.Ui

// One setting: a label with an optional description on the left, its
// control on the right, and a hairline between rows.
Item {
    id: row

    property string label
    property string description
    property bool separator: true
    property bool dimmed: false
    default property alias control: slot.data

    width: parent ? parent.width : 0
    height: Math.max(54, Math.max(texts.implicitHeight, slot.height) + 22)
    opacity: dimmed ? 0.45 : 1

    Rectangle {
        visible: row.separator
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 1
        color: Theme.silverFaint
        opacity: 0.7
    }

    Column {
        id: texts
        anchors { left: parent.left; right: slot.left; rightMargin: 20; verticalCenter: parent.verticalCenter }
        spacing: 3
        Text {
            width: parent.width
            text: row.label
            color: Theme.white
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            wrapMode: Text.WordWrap
        }
        Text {
            visible: row.description.length > 0
            width: parent.width
            text: row.description
            color: Theme.silverDim
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
            lineHeight: 1.1
        }
    }

    Item {
        id: slot
        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
        width: childrenRect.width
        height: childrenRect.height
    }
}
