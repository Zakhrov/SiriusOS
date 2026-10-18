import QtQuick
import Noir.Ui

// Single-line text entry, recessed into the surface. The edge turns white
// while it has focus.
FocusScope {
    id: control

    property alias text: input.text
    property alias echoMode: input.echoMode
    property alias readOnly: input.readOnly
    property alias validator: input.validator
    property alias maximumLength: input.maximumLength
    property alias input: input
    property string placeholderText
    property url iconSource
    property string accessibleName

    signal accepted()

    implicitWidth: 240
    implicitHeight: 32
    activeFocusOnTab: true
    opacity: enabled ? 1 : 0.4

    Accessible.role: Accessible.EditableText
    Accessible.name: accessibleName || placeholderText

    Rectangle {
        anchors.fill: parent
        color: Theme.abyss
        border.width: Theme.edge
        border.color: input.activeFocus ? Theme.white : (mouse.containsMouse ? Theme.silverDim : Theme.silverFaint)
        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
    }
    Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 } height: 1; color: Qt.rgba(0, 0, 0, 0.9) }

    NoirIcon {
        id: icon
        visible: control.iconSource.toString().length > 0
        anchors { left: parent.left; leftMargin: 9; verticalCenter: parent.verticalCenter }
        source: control.iconSource
        size: 14
        color: input.activeFocus ? Theme.white : Theme.silverDim
    }

    TextInput {
        id: input
        anchors { left: icon.visible ? icon.right : parent.left; right: parent.right; verticalCenter: parent.verticalCenter
                  leftMargin: icon.visible ? 8 : 10; rightMargin: 10 }
        focus: true
        clip: true
        color: Theme.white
        selectionColor: Theme.silver
        selectedTextColor: Theme.black
        selectByMouse: true
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontNormal
        onAccepted: control.accepted()

        Text {
            anchors.fill: parent
            visible: !input.text.length && !input.preeditText.length
            text: control.placeholderText
            color: Theme.silverDim
            font: input.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        cursorShape: Qt.IBeamCursor
    }
}
