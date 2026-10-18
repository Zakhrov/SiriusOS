import QtQuick
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    // One staged key, "layout": the list of { name, enabled, width, height,
    // refresh, rotation, primary }, one per connected output. Apply sends it,
    // then asks to keep it and reverts by itself after 15 s, in case the new
    // mode leaves the screen blank; the page cannot be left until then.
    readonly property var layout: page.value("layout") || []
    property var previous: []
    property int selected: 0
    property bool confirming: false
    property int countdown: 0
    readonly property var output: display.outputs[selected] || null
    readonly property var config: layout[selected] || null
    readonly property var rotations: [
        { text: "Normal", degrees: 0 },
        { text: "Rotated left (90°)", degrees: 90 },
        { text: "Upside down (180°)", degrees: 180 },
        { text: "Rotated right (270°)", degrees: 270 },
    ]

    staged: true
    busy: confirming
    busyText: "Keep the new display settings, or let them revert."
    // Defaults: every display on, at its recommended mode, unrotated; the
    // first one primary.
    defaults: ({
        layout: display.outputs.map((o, i) => {
            const best = o.resolutions.find(r => r.width === o.preferredWidth && r.height === o.preferredHeight) || o.resolutions[0]
            return { name: o.name, enabled: true, width: best ? best.width : o.width, height: best ? best.height : o.height,
                     refresh: best && best.rates.length ? best.rates[0] : o.refresh, rotation: 0, primary: i === 0 }
        })
    })
    function current(key) { return display.outputs ? display.currentLayout() : [] }
    function commit(key, value) {
        previous = current("layout")
        const error = display.applyLayout(value)
        if (error.length) {
            host.footerBanner.show(error)
            return
        }
        confirming = true
        countdown = 15
        tick.restart()
        host.footerBanner.show(confirmText(), "info", [{ text: "Keep", action: keep }, { text: "Revert", action: revert }])
    }

    function copy(value) { return JSON.parse(JSON.stringify(value)) }
    function edit(key, value) {
        const next = copy(layout)
        const o = next[selected]
        o[key] = value
        if (key === "primary" && value)
            next.forEach((other, i) => other.primary = i === selected)
        if (key === "resolution") {
            o.width = value.width
            o.height = value.height
            o.refresh = value.rates.length ? value.rates[0] : 0
            delete o.resolution
        }
        if (key === "enabled" && value && !o.width) {
            const best = page.output.resolutions[0]
            o.width = best.width; o.height = best.height; o.refresh = best.rates[0]
        }
        page.stage("layout", next)
    }
    function enabledCount(list) { return list.filter(o => o.enabled).length }
    function confirmText() { return "Keep these display settings? Reverting in " + countdown + " s." }
    function keep() {
        tick.stop()
        confirming = false
        display.saveLayout(display.currentLayout())
        host.footerBanner.show("Saved. These display settings are applied again when you log in.", "info")
    }
    function revert() {
        tick.stop()
        confirming = false
        const error = display.applyLayout(previous)
        host.footerBanner.show(error.length ? error : "The previous display settings are back.", error.length ? "error" : "info")
    }

    DisplayConfig {
        id: display
        onErrorOccurred: (message) => page.host.footerBanner.show(message)
    }

    Timer {
        id: tick
        interval: 1000
        repeat: true
        onTriggered: {
            page.countdown -= 1
            if (page.countdown <= 0)
                page.revert()
            else
                page.host.footerBanner.text = page.confirmText()
        }
    }

    Section {
        title: "Arrangement"
        subtitle: display.available ? "Screen " + display.screenSize + ". Enabled displays sit side by side, left to right. Select one to change it."
                                    : "This X server cannot configure displays (it lacks RandR 1.2)."
        actions: [
            NoirButton {
                text: "Detect displays"
                iconSource: Theme.icon("refresh")
                iconSize: 14
                enabled: display.available && !page.confirming
                onClicked: display.refresh(true)
            }
        ]

        Item {
            id: arrangement
            width: parent.width
            height: 190
            visible: page.layout.length > 0

            readonly property var shown: page.layout.map((o, i) => {
                const sideways = o.rotation === 90 || o.rotation === 270
                const out = display.outputs[i] || {}
                const w = o.enabled ? (sideways ? o.height : o.width) : (out.preferredWidth || 1920)
                const h = o.enabled ? (sideways ? o.width : o.height) : (out.preferredHeight || 1080)
                return { index: i, w: w, h: h, o: o }
            })
            readonly property real totalWidth: shown.reduce((sum, s) => sum + s.w, 0) + (shown.length - 1) * 200
            readonly property real maxHeight: shown.reduce((m, s) => Math.max(m, s.h), 1)
            readonly property real scale: Math.min((width - 40) / Math.max(1, totalWidth), (height - 40) / maxHeight)

            Row {
                anchors.centerIn: parent
                spacing: 200 * arrangement.scale
                Repeater {
                    model: arrangement.shown
                    Item {
                        id: tile
                        required property var modelData
                        readonly property bool current: modelData.index === page.selected
                        width: modelData.w * arrangement.scale
                        height: modelData.h * arrangement.scale
                        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                        opacity: modelData.o.enabled ? 1 : 0.4
                        scale: tileMouse.pressed ? Theme.pressDepth : 1
                        Behavior on width { NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic } }
                        Behavior on height { NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic } }

                        Rectangle {
                            anchors.fill: parent
                            color: tile.current ? Theme.white : (tileMouse.containsMouse ? Theme.panel : Theme.abyss)
                            border.width: Theme.edge
                            border.color: tile.current ? Theme.white : Theme.silver
                            Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 } height: 1; color: Qt.rgba(1, 1, 1, 0.3) }
                        }
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: tile.modelData.o.name
                                color: tile.current ? Theme.black : Theme.white
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontNormal
                                font.bold: true
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: tile.modelData.o.enabled ? tile.modelData.o.width + " × " + tile.modelData.o.height : "Off"
                                color: tile.current ? Theme.black : Theme.silverDim
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: tile.modelData.o.primary
                                text: "PRIMARY"
                                color: tile.current ? Theme.black : Theme.silver
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSmall - 2
                                font.letterSpacing: 2
                            }
                        }
                        MouseArea {
                            id: tileMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: page.selected = tile.modelData.index
                        }
                    }
                }
            }
        }

        // Keyboard access to the selection, for those who don't use the tiles.
        SettingRow {
            visible: page.layout.length > 1
            label: "Display"
            NoirComboBox {
                width: 260
                accessibleName: "Display to configure"
                model: page.layout.map(o => o.name)
                currentIndex: page.selected
                onActivated: (i) => page.selected = i
            }
        }
    }

    Section {
        visible: page.config !== null && page.output !== null
        title: page.output ? page.output.name : ""
        subtitle: page.output && page.output.physicalSize ? page.output.physicalSize : ""

        SettingRow {
            separator: false
            label: "Enabled"
            description: page.config && page.config.enabled && page.enabledCount(page.layout) === 1 ? "The only display that is on cannot be switched off." : ""
            NoirSwitch {
                accessibleName: "Display enabled"
                enabled: !page.confirming && !(page.config && page.config.enabled && page.enabledCount(page.layout) === 1)
                checked: page.config ? page.config.enabled : false
                onToggled: (on) => page.edit("enabled", on)
            }
        }
        SettingRow {
            label: "Primary display"
            description: "Where the taskbar and new windows go."
            dimmed: page.config && !page.config.enabled
            NoirSwitch {
                accessibleName: "Primary display"
                enabled: !page.confirming && page.config && page.config.enabled
                checked: page.config ? page.config.primary : false
                onToggled: (on) => page.edit("primary", on)
            }
        }
        SettingRow {
            label: "Resolution"
            dimmed: page.config && !page.config.enabled
            NoirComboBox {
                width: 260
                accessibleName: "Resolution"
                enabled: !page.confirming && page.config && page.config.enabled
                model: page.output ? page.output.resolutions.map(r => r.width + " × " + r.height
                    + (r.width === page.output.preferredWidth && r.height === page.output.preferredHeight ? "  (recommended)" : "")) : []
                currentIndex: page.output && page.config ? page.output.resolutions.findIndex(r => r.width === page.config.width && r.height === page.config.height) : -1
                onActivated: (i) => page.edit("resolution", page.output.resolutions[i])
            }
        }
        SettingRow {
            id: rateRow
            readonly property var rates: {
                if (!page.output || !page.config)
                    return []
                const r = page.output.resolutions.find(r => r.width === page.config.width && r.height === page.config.height)
                return r ? r.rates : []
            }
            label: "Refresh rate"
            dimmed: page.config && !page.config.enabled
            NoirComboBox {
                width: 260
                accessibleName: "Refresh rate"
                enabled: !page.confirming && page.config && page.config.enabled && rateRow.rates.length > 1
                model: rateRow.rates.map(r => r > 0 ? r.toFixed(2) + " Hz" : "Unknown")
                currentIndex: page.config ? rateRow.rates.findIndex(r => Math.abs(r - page.config.refresh) < 0.02) : -1
                onActivated: (i) => page.edit("refresh", rateRow.rates[i])
            }
        }
        SettingRow {
            label: "Orientation"
            dimmed: page.config && !page.config.enabled
            NoirComboBox {
                width: 260
                accessibleName: "Orientation"
                enabled: !page.confirming && page.config && page.config.enabled
                model: page.rotations.map(r => r.text)
                currentIndex: page.config ? page.rotations.findIndex(r => r.degrees === page.config.rotation) : 0
                onActivated: (i) => page.edit("rotation", page.rotations[i].degrees)
            }
        }

    }

    Section {
        visible: display.backlights.length > 0
        title: "Brightness"

        Repeater {
            model: display.backlights
            SettingRow {
                id: light
                required property var modelData
                required property int index
                separator: index > 0
                label: modelData.name
                description: modelData.type === "raw" ? "Panel backlight" : modelData.type === "firmware" ? "Firmware-controlled backlight" : modelData.type
                Row {
                    spacing: 12
                    NoirSlider {
                        id: level
                        width: 280
                        accessibleName: "Brightness"
                        from: 0.05; to: 1; stepSize: 0.01
                        value: light.modelData.value
                        onMoved: throttle.restart()
                        Timer { id: throttle; interval: 60; onTriggered: display.setBrightness(light.modelData.name, level.current) }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 44
                        text: Math.round(level.current * 100) + "%"
                        color: Theme.silver
                        font.pixelSize: Theme.fontNormal
                    }
                }
            }
        }
    }
}
