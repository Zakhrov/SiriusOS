import QtQuick
import QtQuick.Window
import Noir.Ui

// Drop-down list. The list unfolds in 3D from the button's edge, as an
// overlay on the window's content (so it needs no window of its own), and
// flips upwards when there is no room below. Space/Enter/Alt+Down open it;
// arrows, Home/End and Enter pick; Esc or a click elsewhere closes it.
FocusScope {
    id: control

    // Strings, or objects with a `text` field (any other fields are kept).
    property var model: []
    // The entry to show, normally bound; never assigned by the box itself.
    // A choice shows at once (`shownIndex`) and is reported by activated().
    property int currentIndex: -1
    property int shownIndex: currentIndex
    onCurrentIndexChanged: shownIndex = currentIndex
    property string placeholderText: "Select…"
    property string accessibleName
    property int maxVisibleItems: 10
    readonly property int count: model ? model.length : 0
    readonly property string currentText: textAt(shownIndex)
    readonly property bool highlighted: mouse.containsMouse || activeFocus || opened
    property bool opened: false

    // Emitted for user choices only.
    signal activated(int index)

    function textAt(i) {
        if (!model || i < 0 || i >= model.length)
            return ""
        const entry = model[i]
        return typeof entry === "object" ? String(entry.text) : String(entry)
    }
    function open() {
        if (!enabled || count === 0)
            return
        list.currentIndex = Math.max(0, shownIndex)
        opened = true
        list.forceActiveFocus()
        list.positionViewAtIndex(list.currentIndex, ListView.Contain)
    }
    function close() {
        opened = false
        control.forceActiveFocus()
    }
    function choose(i) {
        close()
        if (i >= 0 && i < count && i !== shownIndex) {
            shownIndex = i
            activated(i)
        }
    }

    implicitWidth: 240
    implicitHeight: 32
    activeFocusOnTab: true
    opacity: enabled ? 1 : 0.4

    Accessible.role: Accessible.ComboBox
    Accessible.name: accessibleName
    Accessible.description: currentText
    Accessible.onPressAction: open()

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                || (event.key === Qt.Key_Down && (event.modifiers & Qt.AltModifier))) {
            open()
        } else if (event.key === Qt.Key_Down && shownIndex + 1 < count) {
            choose(shownIndex + 1)
        } else if (event.key === Qt.Key_Up && shownIndex > 0) {
            choose(shownIndex - 1)
        } else {
            return
        }
        event.accepted = true
    }

    // --- Button --------------------------------------------------------------
    Item {
        id: face
        anchors.fill: parent
        scale: mouse.pressed ? Theme.pressDepth : 1
        Behavior on scale { NumberAnimation { duration: Theme.durationFast } }

        Rectangle {
            anchors.fill: parent
            color: control.highlighted ? Theme.white : Theme.glass
            border.width: Theme.edge
            border.color: control.highlighted ? Theme.white : Theme.glassEdge
            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
        }
        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 1; color: Qt.rgba(1, 1, 1, control.highlighted ? 0.9 : 0.18) }
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Qt.rgba(0, 0, 0, 0.6) }

        Text {
            anchors { left: parent.left; right: chevron.left; leftMargin: 10; rightMargin: 6; verticalCenter: parent.verticalCenter }
            text: control.shownIndex >= 0 ? control.currentText : control.placeholderText
            color: control.highlighted ? Theme.black : (control.shownIndex >= 0 ? Theme.silver : Theme.silverDim)
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            font.letterSpacing: Theme.letterSpacing
            elide: Text.ElideRight
        }
        NoirIcon {
            id: chevron
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            source: Theme.icon("chevron-down")
            size: 14
            color: control.highlighted ? Theme.black : Theme.silver
            rotation: control.opened ? 180 : 0
            Behavior on rotation { NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic } }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            control.forceActiveFocus(Qt.MouseFocusReason)
            control.opened ? control.close() : control.open()
        }
        onWheel: (wheel) => {
            if (!control.activeFocus) { wheel.accepted = false; return }
            const next = control.shownIndex + (wheel.angleDelta.y > 0 ? -1 : 1)
            if (next >= 0 && next < control.count)
                control.choose(next)
        }
    }

    // --- Overlay -------------------------------------------------------------
    Item {
        id: overlay
        parent: control.Window.contentItem
        anchors.fill: parent
        z: 10000
        visible: control.opened || popup.opacity > 0.01

        // A click anywhere else closes the list.
        MouseArea {
            anchors.fill: parent
            enabled: control.opened
            acceptedButtons: Qt.AllButtons
            onPressed: control.close()
            onWheel: (wheel) => wheel.accepted = true
        }

        Item {
            id: popup
            readonly property point origin: control.opened || visible ? control.mapToItem(overlay, 0, 0) : Qt.point(0, 0)
            readonly property int rows: Math.min(control.count, control.maxVisibleItems)
            readonly property bool above: origin.y + control.height + height + 8 > overlay.height
                                          && origin.y - height - 4 >= 0
            x: Math.max(4, Math.min(origin.x, overlay.width - width - 4))
            y: above ? origin.y - height - 2 : origin.y + control.height + 2
            width: control.width
            height: rows * 30 + 8
            opacity: control.opened ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.durationNormal } }
            transform: Rotation {
                origin.x: popup.width / 2
                origin.y: popup.above ? popup.height : 0
                axis { x: 1; y: 0; z: 0 }
                angle: control.opened ? 0 : (popup.above ? 90 : -90)
                Behavior on angle { NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic } }
            }

            GlassPanel {
                anchors.fill: parent
                color: Theme.glassStrong
                elevation: 10
            }

            ListView {
                id: list
                anchors { fill: parent; margins: 4; rightMargin: scrollBar.visible ? 12 : 4 }
                clip: true
                model: control.model
                boundsBehavior: Flickable.StopAtBounds
                highlightMoveDuration: 0
                keyNavigationEnabled: true

                delegate: Item {
                    id: row
                    required property int index
                    required property var modelData
                    readonly property bool current: ListView.isCurrentItem
                    width: ListView.view.width
                    height: 30
                    Rectangle {
                        anchors.fill: parent
                        color: row.current ? Theme.white : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                    }
                    NoirIcon {
                        visible: row.index === control.shownIndex
                        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                        source: Theme.icon("check")
                        size: 12
                        color: row.current ? Theme.black : Theme.silver
                    }
                    Text {
                        anchors { left: parent.left; right: parent.right; leftMargin: 28; rightMargin: 8; verticalCenter: parent.verticalCenter }
                        text: control.textAt(row.index)
                        color: row.current ? Theme.black : Theme.silver
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: list.currentIndex = row.index
                        onClicked: control.choose(row.index)
                    }
                }

                Keys.onPressed: (event) => {
                    switch (event.key) {
                    case Qt.Key_Return: case Qt.Key_Enter: case Qt.Key_Space: control.choose(currentIndex); break
                    case Qt.Key_Escape: case Qt.Key_Tab: control.close(); break
                    case Qt.Key_Home: currentIndex = 0; break
                    case Qt.Key_End: currentIndex = count - 1; break
                    default: return
                    }
                    event.accepted = true
                }
            }

            NoirScrollBar {
                id: scrollBar
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom; margins: 4 }
                flickable: list
            }
        }
    }
}
