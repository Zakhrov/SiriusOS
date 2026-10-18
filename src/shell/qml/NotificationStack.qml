import QtQuick
import QtQuick.Window
import Noir.Shell
import Noir.Ui

// Bottom-right stack of notification cards. New cards slide in from the
// corner with a 3D swing; dismissed cards slide back out to the right.
Item {
    id: stack

    // Updated imperatively: the window's height follows this, which resizes
    // the list, which re-lays out its content; a binding would loop.
    property int contentHeight: 1
    function fitContent() {
        contentHeight = Math.max(1, Math.min(list.contentHeight, Screen.height - Theme.taskbarHeight - 24))
    }
    width: 380
    height: contentHeight

    // The cards' rectangles in `target`'s coordinates, for blur behind them.
    function cardRects(target) {
        const rects = []
        for (const card of list.contentItem.children) {
            if (!card.visible || card.width <= 0 || card.height <= 0 || card.opacity < 0.05)
                continue
            const p = card.mapToItem(target, 0, 0)
            if (p.y + card.height < 0 || p.y > target.height)
                continue
            const rect = Qt.rect(Math.round(p.x), Math.round(p.y), Math.round(card.width), Math.round(card.height))
            // The list may keep a pooled copy of a delegate around.
            if (!rects.some(r => r.x === rect.x && r.y === rect.y && r.width === rect.width && r.height === rect.height))
                rects.push(rect)
        }
        return rects
    }

    ListView {
        id: list
        anchors.fill: parent
        model: NotificationServer
        spacing: 10
        clip: true
        interactive: contentHeight > height
        verticalLayoutDirection: ListView.BottomToTop
        onContentHeightChanged: Qt.callLater(stack.fitContent)

        delegate: NotificationCard {
            required notificationId
            required appName
            required summary
            required body
            required iconName
            required urgency
            required actions
            required progress
            x: stack.width - width - 10
        }

        add: Transition {
            ParallelAnimation {
                NumberAnimation { property: "x"; from: stack.width + 40; duration: Theme.durationSlow; easing.type: Easing.OutCubic }
                NumberAnimation { property: "swing"; from: 55; to: 0; duration: Theme.durationSlow; easing.type: Easing.OutBack }
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationNormal }
            }
        }
        remove: Transition {
            ParallelAnimation {
                NumberAnimation { property: "x"; to: stack.width + 40; duration: Theme.durationNormal; easing.type: Easing.InCubic }
                NumberAnimation { property: "swing"; to: -40; duration: Theme.durationNormal; easing.type: Easing.InCubic }
                NumberAnimation { property: "opacity"; to: 0; duration: Theme.durationNormal }
            }
        }
        displaced: Transition {
            NumberAnimation { property: "y"; duration: Theme.durationNormal; easing.type: Easing.OutCubic }
        }
    }
}
