import QtQuick
import Noir.Ui

// Read-only "label: value" line. The value can be selected and copied.
Item {
    id: row

    property string label
    property string value
    property bool mono: false

    width: parent ? parent.width : 0
    height: Math.max(34, valueText.contentHeight + 14)
    visible: value.length > 0

    Text {
        id: labelText
        anchors { left: parent.left; top: parent.top; topMargin: 8 }
        width: Math.min(200, parent.width * 0.32)
        text: row.label
        color: Theme.silverDim
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontNormal
        elide: Text.ElideRight
    }
    TextEdit {
        id: valueText
        anchors { left: labelText.right; right: parent.right; top: parent.top; topMargin: 8; leftMargin: 12 }
        text: row.value
        readOnly: true
        selectByMouse: true
        activeFocusOnTab: false
        wrapMode: TextEdit.Wrap
        color: Theme.white
        selectionColor: Theme.silver
        selectedTextColor: Theme.black
        font.family: row.mono ? Theme.monoFamily : Theme.fontFamily
        font.pixelSize: Theme.fontNormal
    }
}
