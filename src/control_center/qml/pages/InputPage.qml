import QtQuick
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    readonly property var switchOptions: [
        { text: "None", option: "" },
        { text: "Alt+Shift", option: "grp:alt_shift_toggle" },
        { text: "Ctrl+Shift", option: "grp:ctrl_shift_toggle" },
        { text: "Super+Space", option: "grp:win_space_toggle" },
        { text: "Caps Lock", option: "grp:caps_toggle" },
        { text: "Both Shift keys", option: "grp:shifts_toggle" },
    ]
    readonly property var layoutNames: {
        const names = {}
        for (const l of input.availableLayouts)
            names[l.code] = l.text
        return names
    }
    // libinput's own defaults, for the options the connected devices have.
    readonly property var pointerDefaults: ({
        accelSpeed: 0, flatProfile: false, naturalScroll: false, leftHanded: false,
        middleEmulation: false, tapping: true, disableWhileTyping: true,
    })

    // Keys: "repeat" ({ enabled, delay, rate }), "layouts" ({ layouts, switch }),
    // and "mouse/<option>" / "touchpad/<option>" for pointer options.
    staged: true
    defaults: {
        const d = { repeat: { enabled: true, delay: 600, rate: 25 } }
        for (const key in input.mouse) d["mouse/" + key] = pointerDefaults[key]
        for (const key in input.touchpad) d["touchpad/" + key] = pointerDefaults[key]
        return d
    }
    function current(key) {
        if (key === "repeat")
            return { enabled: input.repeatEnabled, delay: input.repeatDelay, rate: input.repeatRate }
        if (key === "layouts")
            return { layouts: input.layouts, switch: input.layoutSwitch }
        const [kind, option] = key.split("/")
        return (kind === "touchpad" ? input.touchpad : input.mouse)[option]
    }
    function commit(key, value) {
        if (key === "repeat")
            input.setRepeat(value.enabled, value.delay, value.rate)
        else if (key === "layouts")
            input.setLayouts(value.layouts, value.switch)
        else {
            const [kind, option] = key.split("/")
            input.setPointerSetting(kind === "touchpad", option, value)
        }
    }
    function stageRepeat(field, v) {
        const repeat = Object.assign({}, value("repeat"))
        repeat[field] = v
        stage("repeat", repeat)
    }
    function stageLayouts(layouts, option) {
        stage("layouts", { layouts: layouts, switch: layouts.length > 1 ? option : "" })
    }

    InputConfig {
        id: input
        onErrorOccurred: (message) => banner.show(message)
    }

    // Pick up devices plugged in while the page is open.
    Timer {
        interval: 5000
        repeat: true
        running: page.active
        onTriggered: input.refresh()
    }

    // The libinput options shared by mice and touchpads.
    component PointerSection: Section {
        id: section
        property bool touchpad
        property var settings: ({})
        readonly property string kind: touchpad ? "touchpad/" : "mouse/"
        readonly property var names: input.pointers.filter(p => p.touchpad === section.touchpad).map(p => p.name)
        subtitle: names.join(", ")
        function has(option) { return settings[option] !== undefined }
        function get(option) { return page.value(kind + option) }
        function set(option, v) { page.stage(kind + option, v) }

        SettingRow {
            separator: false
            visible: section.has("accelSpeed")
            label: "Pointer speed"
            Row {
                spacing: 12
                Text { anchors.verticalCenter: parent.verticalCenter; text: "Slow"; color: Theme.silverDim; font.pixelSize: Theme.fontSmall }
                NoirSlider {
                    width: 260
                    accessibleName: "Pointer speed"
                    from: -1; to: 1; stepSize: 0.05
                    value: section.has("accelSpeed") ? section.get("accelSpeed") : 0
                    onMoved: (v) => section.set("accelSpeed", Math.round(v * 100) / 100)
                }
                Text { anchors.verticalCenter: parent.verticalCenter; text: "Fast"; color: Theme.silverDim; font.pixelSize: Theme.fontSmall }
            }
        }
        SettingRow {
            visible: section.has("flatProfile")
            label: "Acceleration"
            description: "Moving faster takes the pointer further. Off gives a constant, one-to-one feel."
            NoirSwitch {
                accessibleName: "Pointer acceleration"
                checked: section.get("flatProfile") === false
                onToggled: (on) => section.set("flatProfile", !on)
            }
        }
        SettingRow {
            visible: section.has("tapping")
            label: "Tap to click"
            NoirSwitch {
                accessibleName: "Tap to click"
                checked: !!section.get("tapping")
                onToggled: (on) => section.set("tapping", on)
            }
        }
        SettingRow {
            visible: section.has("naturalScroll")
            label: "Natural scrolling"
            description: "Content follows your fingers, as on a phone."
            NoirSwitch {
                accessibleName: "Natural scrolling"
                checked: !!section.get("naturalScroll")
                onToggled: (on) => section.set("naturalScroll", on)
            }
        }
        SettingRow {
            visible: section.has("disableWhileTyping")
            label: "Disable while typing"
            NoirSwitch {
                accessibleName: "Disable while typing"
                checked: !!section.get("disableWhileTyping")
                onToggled: (on) => section.set("disableWhileTyping", on)
            }
        }
        SettingRow {
            visible: section.has("leftHanded")
            label: "Left-handed"
            description: "Swap the left and right buttons."
            NoirSwitch {
                accessibleName: "Left-handed"
                checked: !!section.get("leftHanded")
                onToggled: (on) => section.set("leftHanded", on)
            }
        }
        SettingRow {
            visible: section.has("middleEmulation")
            label: "Middle click"
            description: "Pressing left and right together clicks the middle button."
            NoirSwitch {
                accessibleName: "Middle click emulation"
                checked: !!section.get("middleEmulation")
                onToggled: (on) => section.set("middleEmulation", on)
            }
        }
    }

    Banner { id: banner }

    Section {
        title: "Keyboard"
        subtitle: "Saved, and applied again each time you log in."

        SettingRow {
            separator: false
            label: "Key repeat"
            description: "Holding a key down types it again and again."
            NoirSwitch {
                accessibleName: "Key repeat"
                checked: page.value("repeat").enabled
                onToggled: (on) => page.stageRepeat("enabled", on)
            }
        }
        SettingRow {
            label: "Delay"
            description: "How long to hold before repeating starts."
            dimmed: !page.value("repeat").enabled
            Row {
                spacing: 12
                NoirSlider {
                    id: delay
                    width: 260
                    accessibleName: "Repeat delay"
                    enabled: page.value("repeat").enabled
                    from: 150; to: 1000; stepSize: 10
                    value: page.value("repeat").delay
                    onMoved: (v) => page.stageRepeat("delay", Math.round(v))
                }
                Text { anchors.verticalCenter: parent.verticalCenter; width: 60; text: Math.round(delay.current) + " ms"; color: Theme.silver; font.pixelSize: Theme.fontNormal }
            }
        }
        SettingRow {
            label: "Rate"
            dimmed: !page.value("repeat").enabled
            Row {
                spacing: 12
                NoirSlider {
                    id: rate
                    width: 260
                    accessibleName: "Repeat rate"
                    enabled: page.value("repeat").enabled
                    from: 5; to: 60; stepSize: 1
                    value: page.value("repeat").rate
                    onMoved: (v) => page.stageRepeat("rate", Math.round(v))
                }
                Text { anchors.verticalCenter: parent.verticalCenter; width: 60; text: Math.round(rate.current) + " /s"; color: Theme.silver; font.pixelSize: Theme.fontNormal }
            }
        }
        SettingRow {
            label: "Test"
            description: "Apply first to try new repeat settings here."
            NoirTextField {
                width: 320
                placeholderText: "Hold a key down here"
            }
        }
    }

    Section {
        title: "Layouts"
        subtitle: input.layoutsSupported ? "Up to four. The first is the default." : "setxkbmap is not installed, so layouts cannot be changed."

        Repeater {
            model: page.value("layouts").layouts
            SettingRow {
                id: layoutRow
                required property string modelData
                required property int index
                readonly property var layouts: page.value("layouts")
                separator: index > 0
                label: page.layoutNames[modelData] || modelData
                description: index === 0 ? "Default" : ""
                NoirButton {
                    text: "Remove"
                    enabled: layoutRow.layouts.layouts.length > 1 && input.layoutsSupported
                    onClicked: page.stageLayouts(layoutRow.layouts.layouts.filter((l, i) => i !== layoutRow.index),
                                                 layoutRow.layouts.switch)
                }
            }
        }
        SettingRow {
            id: addRow
            readonly property var layouts: page.value("layouts")
            label: "Add a layout"
            dimmed: layouts.layouts.length >= 4 || !input.layoutsSupported
            Row {
                spacing: 8
                NoirComboBox {
                    id: newLayout
                    width: 320
                    accessibleName: "Layout to add"
                    enabled: addRow.layouts.layouts.length < 4 && input.layoutsSupported
                    maxVisibleItems: 12
                    model: input.availableLayouts.map(l => l.text)
                }
                NoirButton {
                    text: "Add"
                    enabled: newLayout.shownIndex >= 0 && addRow.layouts.layouts.length < 4 && input.layoutsSupported
                    onClicked: {
                        const code = input.availableLayouts[newLayout.shownIndex].code
                        if (addRow.layouts.layouts.indexOf(code) < 0)
                            page.stageLayouts(addRow.layouts.layouts.concat([code]), addRow.layouts.switch || "grp:alt_shift_toggle")
                    }
                }
            }
        }
        SettingRow {
            id: switchRow
            readonly property var layouts: page.value("layouts")
            visible: layouts.layouts.length > 1
            label: "Switch layouts with"
            NoirComboBox {
                width: 200
                accessibleName: "Switch layouts with"
                model: page.switchOptions.map(o => o.text)
                currentIndex: Math.max(0, page.switchOptions.findIndex(o => o.option === switchRow.layouts.switch))
                onActivated: (i) => page.stageLayouts(switchRow.layouts.layouts, page.switchOptions[i].option)
            }
        }
    }

    PointerSection {
        visible: Object.keys(input.mouse).length > 0
        title: "Mouse"
        touchpad: false
        settings: input.mouse
    }

    PointerSection {
        visible: Object.keys(input.touchpad).length > 0
        title: "Touchpad"
        touchpad: true
        settings: input.touchpad
    }

    Section {
        visible: Object.keys(input.mouse).length === 0 && Object.keys(input.touchpad).length === 0
        title: "Pointing devices"
        Text {
            width: parent.width
            bottomPadding: 12
            text: "No mouse or touchpad driven by libinput was found, so there is nothing to adjust."
            color: Theme.silverDim
            font.pixelSize: Theme.fontNormal
            wrapMode: Text.WordWrap
        }
    }
}
