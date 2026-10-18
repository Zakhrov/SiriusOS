import QtQuick
import Noir.Ui
import Noir.ControlCenter

SettingsPage {
    id: page

    // Keys are category ids; values are application ids (the terminal's is a command).
    staged: true
    defaults: ({ terminal: "x-terminal-emulator" })
    function category(key) { return apps.categories.find(c => c.id === key) }
    function current(key) { const c = category(key); return c ? c.current : "" }
    function commit(key, value) { apps.setDefault(key, value) }

    DefaultApps {
        id: apps
        onErrorOccurred: (message) => banner.show(message)
    }

    Banner { id: banner }

    Section {
        title: "Default applications"
        subtitle: "Used when you open a link or a file, and by the desktop's Terminal launcher."

        Repeater {
            model: apps.categories
            SettingRow {
                id: row
                required property var modelData
                required property int index
                separator: index > 0
                label: modelData.title
                description: modelData.candidates.length === 0 ? "No installed application handles this." : ""

                Row {
                    spacing: 10
                    NoirIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        source: Theme.icon(row.modelData.icon)
                        size: 16
                        color: Theme.silverDim
                    }
                    NoirComboBox {
                        width: 300
                        accessibleName: row.modelData.title
                        enabled: row.modelData.candidates.length > 0
                        placeholderText: "Not set"
                        model: row.modelData.candidates.map(c => c.name)
                        currentIndex: row.modelData.candidates.findIndex(c => c.id === page.value(row.modelData.id))
                        onActivated: (i) => page.stage(row.modelData.id, row.modelData.candidates[i].id)
                    }
                }
            }
        }
    }
}
