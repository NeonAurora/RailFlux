import QtQuick
import "../components"
import "../data/StationData.js" as StationData

Rectangle {
    id: stationLayout
    color: "#1e1e1e"  // Your BG_COLOR from settings.py

    property var dbManager
    property int cellSize: Math.floor(width / 250)  // GRID_COLUMNS from settings.py
    property bool showGrid: true

    // Basic click handlers
    function handleTrackClick(segmentId, currentState) {
        console.log("Track segment clicked:", segmentId, "Currently occupied:", currentState)
        // Future: Toggle occupancy via database
    }

    function handleSignalClick(signalId) {
        console.log("Signal clicked:", signalId)
        // Future: Open signal control interface
    }

    function updateDisplay() {
        console.log("Station display updated from database")
    }

    // Main grid canvas
    GridCanvas {
        id: canvas
        anchors.fill: parent
        gridSize: stationLayout.cellSize
        showGrid: stationLayout.showGrid

        // Track segments - core layout structure
        Repeater {
            model: StationData.trackSegments

            TrackSegment {
                segmentId: modelData.id
                startRow: modelData.startRow
                startCol: modelData.startCol
                endRow: modelData.endRow
                endCol: modelData.endCol
                cellSize: stationLayout.cellSize

                // Track circuit state (ON/OFF - regardless of hardware implementation)
                isOccupied: {
                    if (!stationLayout.dbManager) return modelData.occupied
                    // Future: database polling will update this
                    return modelData.occupied
                }

                onTrackClicked: stationLayout.handleTrackClick(segmentId, currentState)
            }
        }

        // Basic signals (simple representation for now)
        Repeater {
            model: StationData.signalPositions

            BasicSignal {
                x: modelData.col * stationLayout.cellSize
                y: modelData.row * stationLayout.cellSize
                signalId: modelData.id
                signalName: modelData.name
                cellSize: stationLayout.cellSize

                currentAspect: {
                    if (!stationLayout.dbManager) return modelData.aspect
                    // Future: database polling will update this
                    return modelData.aspect
                }

                onSignalClicked: stationLayout.handleSignalClick(signalId)
            }
        }

        // Text labels (your text_config.txt system)
        Repeater {
            model: StationData.textLabels

            Text {
                x: modelData.col * stationLayout.cellSize
                y: modelData.row * stationLayout.cellSize
                text: modelData.text
                color: "#ffffff"
                font.pixelSize: modelData.fontSize || 12
                font.family: "Arial"
            }
        }

        // Level crossings (basic representation)
        Repeater {
            model: StationData.levelCrossings

            LevelCrossingGate {
                x: modelData.col * stationLayout.cellSize
                y: modelData.row * stationLayout.cellSize
                gateId: modelData.id
                gateName: modelData.name
                cellSize: stationLayout.cellSize

                gateState: {
                    if (!stationLayout.dbManager) return modelData.state
                    return modelData.state
                }

                onGateClicked: console.log("Level crossing clicked:", gateId)
            }
        }
    }

    // Status panel (matching your pygame debug style)
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 10
        width: 220
        height: 100
        color: "#2d3748"
        border.color: "#4a5568"
        border.width: 1
        radius: 4

        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 5

            Text {
                text: "Grid: " + stationLayout.cellSize + "px"
                color: "#ffffff"
                font.pixelSize: 12
            }

            Text {
                text: "Layout: 250×200 (W×H)"
                color: "#a0aec0"
                font.pixelSize: 10
            }

            Text {
                text: "Track Segments: " + StationData.trackSegments.length
                color: "#a0aec0"
                font.pixelSize: 10
            }

            Rectangle {
                width: parent.width
                height: 25
                color: gridToggle.pressed ? "#2c5aa0" : "#3182ce"
                radius: 4

                Text {
                    anchors.centerIn: parent
                    text: "Toggle Grid"
                    color: "white"
                    font.pixelSize: 11
                }

                MouseArea {
                    id: gridToggle
                    anchors.fill: parent
                    onClicked: stationLayout.showGrid = !stationLayout.showGrid
                }
            }
        }
    }
}
