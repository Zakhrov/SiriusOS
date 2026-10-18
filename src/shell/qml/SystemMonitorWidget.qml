import QtQuick
import QtQuick.Shapes
import Noir.Shell
import Noir.Ui

FloatingWidget {
    id: monitor

    title: "System"
    width: 300
    height: 236
    phase: 900

    function formatUptime(seconds) {
        const d = Math.floor(seconds / 86400)
        const h = Math.floor(seconds % 86400 / 3600)
        const m = Math.floor(seconds % 3600 / 60)
        return (d > 0 ? d + "d " : "") + h + "h " + m + "m"
    }

    Column {
        anchors.fill: parent
        spacing: 10

        Text {
            text: SystemMonitor.userName + "@" + SystemMonitor.hostName
            color: Theme.white
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontNormal
        }

        // CPU history trace.
        Item {
            width: parent.width
            height: 56

            Rectangle { anchors.fill: parent; color: Theme.abyss; border.color: Theme.silverFaint }
            Repeater {
                model: 3
                Rectangle { x: 1; width: parent.width - 2; y: (index + 1) * parent.height / 4; height: 1; color: Theme.silverFaint }
            }
            Shape {
                id: trace
                anchors.fill: parent
                antialiasing: true
                ShapePath {
                    strokeColor: Theme.silver
                    strokeWidth: 1.5
                    fillColor: Qt.rgba(0.76, 0.78, 0.82, 0.12)
                    startX: 0; startY: trace.height
                    PathPolyline {
                        path: {
                            const h = SystemMonitor.cpuHistory
                            const pts = [Qt.point(0, trace.height)]
                            for (let i = 0; i < h.length; ++i)
                                pts.push(Qt.point(i * trace.width / (h.length - 1), trace.height - 2 - h[i] * (trace.height - 4)))
                            pts.push(Qt.point(trace.width, trace.height))
                            return pts
                        }
                    }
                }
            }
            Text {
                anchors { right: parent.right; top: parent.top; margins: 4 }
                text: "CPU " + Math.round(SystemMonitor.cpuLoad * 100) + "%"
                color: Theme.white
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontSmall
            }
        }

        Column {
            width: parent.width
            spacing: 4
            Text {
                text: "MEMORY  " + SystemMonitor.memoryUsedMiB + " / " + SystemMonitor.memoryTotalMiB + " MiB"
                color: Theme.silverDim
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontSmall
            }
            ProgressBar3D { width: parent.width; value: SystemMonitor.memoryLoad }
        }

        Text {
            text: "UPTIME  " + monitor.formatUptime(SystemMonitor.uptimeSeconds)
            color: Theme.silverDim
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontSmall
        }
    }
}
