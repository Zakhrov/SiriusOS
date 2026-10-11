import QtQuick
import QtQuick.Window
import Noir.Shell

// Bottom-right stack of notification cards. New cards slide in from the
// corner with a 3D swing; dismissed cards slide back out to the right.
Item {
    id: stack

    readonly property int contentHeight: Math.min(list.contentHeight, Math.max(1, Screen.height - Theme.taskbarHeight - 24))
    width: 380
    height: contentHeight

    ListView {
        id: list
        anchors.fill: parent
        model: NotificationServer
        spacing: 10
        clip: true
        interactive: contentHeight > height
        verticalLayoutDirection: ListView.BottomToTop

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
