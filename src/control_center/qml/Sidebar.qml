import QtQuick
import Noir.Ui
import Noir.ControlCenter

// Category list with search. Up/Down (or typing in the search field and
// pressing Down) moves between pages, Enter or Tab goes into the page.
FocusScope {
    id: sidebar

    property var pages: []
    property string currentId

    signal selected(string id)
    signal enterPage()

    function focusSearch() {
        search.forceActiveFocus(Qt.ShortcutFocusReason)
        search.input.selectAll()
    }
    function matches(page, words) {
        const haystack = (page.title + " " + page.description + " " + page.keywords + " " + page.group).toLowerCase()
        return words.every(w => haystack.indexOf(w) >= 0)
    }
    function rebuild() {
        const words = search.text.toLowerCase().split(/\s+/).filter(w => w.length > 0)
        filtered.clear()
        for (const page of pages) {
            if (matches(page, words))
                filtered.append({ pageId: page.id, title: page.title, icon: page.icon, group: page.group })
        }
        list.syncCurrent()
    }
    function move(step) {
        if (filtered.count === 0)
            return
        const next = Math.max(0, Math.min(filtered.count - 1, list.currentIndex + step))
        sidebar.selected(filtered.get(next).pageId)
    }

    onPagesChanged: rebuild()
    onCurrentIdChanged: list.syncCurrent()

    ListModel { id: filtered }

    GlassPanel {
        anchors.fill: parent
        color: Theme.glassStrong
        elevation: 8
    }

    Column {
        id: top
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
        spacing: 14

        Row {
            spacing: 10
            NoirIcon { source: Theme.icon("settings"); size: 22; color: Theme.white; anchors.verticalCenter: parent.verticalCenter }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    text: "CONTROL CENTER"
                    color: Theme.white
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontNormal
                    font.letterSpacing: 3
                }
                Text {
                    text: "SiriusOS · Noir " + Qt.application.version
                    color: Theme.silverDim
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.letterSpacing: 1
                }
            }
        }

        NoirTextField {
            id: search
            width: parent.width
            placeholderText: "Search settings"
            iconSource: Theme.icon("search")
            accessibleName: "Search settings"
            onTextChanged: sidebar.rebuild()
            onAccepted: if (filtered.count > 0) { sidebar.selected(filtered.get(0).pageId); sidebar.enterPage() }
            Keys.onDownPressed: list.forceActiveFocus(Qt.TabFocusReason)
            Keys.onEscapePressed: (event) => { if (text.length) { text = ""; event.accepted = true } else event.accepted = false }
        }
    }

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: top.bottom; bottom: parent.bottom
                  leftMargin: 8; rightMargin: 8; topMargin: 14; bottomMargin: 8 }
        clip: true
        model: filtered
        activeFocusOnTab: true
        boundsBehavior: Flickable.StopAtBounds
        highlightMoveDuration: Theme.durationFast
        keyNavigationEnabled: false

        function syncCurrent() {
            for (let i = 0; i < filtered.count; ++i) {
                if (filtered.get(i).pageId === sidebar.currentId) {
                    currentIndex = i
                    return
                }
            }
            currentIndex = -1
        }

        Keys.onPressed: (event) => {
            switch (event.key) {
            case Qt.Key_Down: sidebar.move(1); break
            case Qt.Key_Up:
                if (currentIndex <= 0) search.forceActiveFocus(Qt.BacktabFocusReason)
                else sidebar.move(-1)
                break
            case Qt.Key_Home: if (filtered.count) sidebar.selected(filtered.get(0).pageId); break
            case Qt.Key_End: if (filtered.count) sidebar.selected(filtered.get(filtered.count - 1).pageId); break
            case Qt.Key_Return: case Qt.Key_Enter: case Qt.Key_Right: sidebar.enterPage(); break
            default: return
            }
            event.accepted = true
        }

        section.property: "group"
        section.delegate: Item {
            required property string section
            width: ListView.view.width
            height: 34
            Text {
                anchors { left: parent.left; leftMargin: 10; bottom: parent.bottom; bottomMargin: 6 }
                text: parent.section.toUpperCase()
                color: Theme.silverDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall - 1
                font.letterSpacing: 2.5
            }
        }

        delegate: Item {
            id: entry
            required property int index
            required property string pageId
            required property string title
            required property string icon
            readonly property bool current: pageId === sidebar.currentId
            readonly property bool highlighted: current || mouse.containsMouse || (list.activeFocus && ListView.isCurrentItem)
            width: ListView.view.width
            height: 40

            Accessible.role: Accessible.PageTab
            Accessible.name: title
            Accessible.selected: current

            Rectangle {
                anchors.fill: parent
                color: entry.current ? Theme.white : (entry.highlighted ? Qt.rgba(1, 1, 1, 0.1) : "transparent")
                border.width: list.activeFocus && entry.current ? 1 : 0
                border.color: Theme.black
                scale: mouse.pressed ? Theme.pressDepth : 1
                Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                Behavior on scale { NumberAnimation { duration: Theme.durationFast } }
            }
            // Current page marker: a silver blade on the left edge.
            Rectangle {
                visible: entry.current
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                width: 3
                color: Theme.black
            }
            NoirIcon {
                anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
                source: Theme.icon(entry.icon)
                size: 18
                color: entry.current ? Theme.black : Theme.silver
            }
            Text {
                anchors { left: parent.left; leftMargin: 46; right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                text: entry.title
                color: entry.current ? Theme.black : Theme.silver
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                font.letterSpacing: 0.5
                elide: Text.ElideRight
            }
            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    list.forceActiveFocus(Qt.MouseFocusReason)
                    sidebar.selected(entry.pageId)
                }
            }
        }

        Text {
            anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 20 }
            visible: filtered.count === 0
            text: "No settings match"
            color: Theme.silverDim
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
        }
    }
}
