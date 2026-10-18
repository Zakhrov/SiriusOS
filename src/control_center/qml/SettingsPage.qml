import QtQuick

// Root of every Control Center page: a column of sections. `host` is the
// Control Center window (to switch pages, or to show a message in its
// footer); `active` is true while the page is on screen, so pages can poll
// only then.
//
// Staged pages (`staged: true`) do not change anything until Apply: their
// controls show value(key) and call stage(key, value), and the window's
// footer offers Defaults, Reset and Apply. Such a page implements
//   function current(key)          what the setting is now
//   function commit(key, value)    make it so (on Apply)
// and lists the keys Defaults restores in `defaults` ({ key: value }).
// Pages without staging act at once (volume, Wi-Fi, pairing, ...).
Column {
    id: page

    property var host
    property bool active: visible && Window.window && Window.window.visible

    property bool staged: false
    // While true (e.g. a display change waiting to be kept), the page cannot
    // be left and nothing can be applied; `busyText` says why.
    property bool busy: false
    property string busyText
    property var defaults: ({})
    property var pending: ({})
    readonly property bool hasChanges: Object.keys(pending).length > 0
    readonly property bool atDefaults: Object.keys(defaults).every(key => same(value(key), defaults[key]))

    // After Apply has committed every staged change.
    signal committed()

    function same(a, b) { return JSON.stringify(a) === JSON.stringify(b) }
    function current(key) { return undefined }
    function commit(key, value) {}

    // What a control should show: the staged value, else the current one.
    function value(key) { return pending.hasOwnProperty(key) ? pending[key] : current(key) }
    function stage(key, newValue) {
        const next = Object.assign({}, pending)
        if (same(newValue, current(key)))
            delete next[key]
        else
            next[key] = newValue
        pending = next
    }
    function apply() {
        const changes = pending
        pending = ({})
        for (const key in changes)
            commit(key, changes[key])
        committed()
    }
    function reset() { pending = ({}) }
    function restoreDefaults() {
        for (const key in defaults)
            stage(key, defaults[key])
    }

    width: parent ? parent.width : 0
    spacing: 18
}
