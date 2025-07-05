import QtQuick
import "../components"
import "../data/StationData.js" as StationData

Rectangle {
    id: stationLayout
    color: "#1e1e1e"

    property var dbManager
    property int cellSize: Math.floor(width / 320)
    property bool showGrid: true

    property real globalRowPadding: 1
    property real globalColPadding: 1

    // Basic click handlers
    function handleTrackClick(segmentId, currentState) {
        console.log("Track segment clicked:", segmentId, "Currently occupied:", currentState)
    }

    function handleSignalClick(signalId) {
        console.log("Signal clicked:", signalId)
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

                rowPadding: stationLayout.globalRowPadding
                colPadding: stationLayout.globalColPadding

                isOccupied: {
                    if (!stationLayout.dbManager) return modelData.occupied
                    return modelData.occupied
                }

                onTrackClicked: stationLayout.handleTrackClick(segmentId, currentState)
            }
        }

        // Basic signals
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
                    return modelData.aspect
                }

                onSignalClicked: stationLayout.handleSignalClick(signalId)
            }
        }

        // Text labels
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

        // Level crossings
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

    // Replace your entire statusPanel Rectangle with this minimal version:
    // **ENHANCED EXPANDABLE STATUS PANEL** with Dynamic Grid + Resize
    Rectangle {
        id: statusPanel
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 10

        // **EXPANDABLE PROPERTIES**
        property bool isExpanded: false
        property int collapsedWidth: 220
        property int collapsedHeight: 60
        property int expandedWidth: 400
        property int expandedHeight: 350

        // **RESIZABLE PROPERTIES**
        property int minWidth: 300
        property int maxWidth: 600
        property int minHeight: 250
        property int maxHeight: 500

        width: isExpanded ? expandedWidth : collapsedWidth
        height: isExpanded ? expandedHeight : collapsedHeight

        color: "#2d3748"
        border.color: isExpanded ? "#3182ce" : "#4a5568"
        border.width: 2
        radius: 6

        // **SMOOTH ANIMATIONS**
        Behavior on width {
            NumberAnimation {
                duration: 300
                easing.type: Easing.OutQuart
            }
        }

        Behavior on height {
            NumberAnimation {
                duration: 300
                easing.type: Easing.OutQuart
            }
        }

        Behavior on border.color {
            ColorAnimation { duration: 300 }
        }

        // **EXPAND/COLLAPSE BUTTON** - Top-left corner
        Rectangle {
            id: expandButton
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 8
            width: 24
            height: 24
            color: expandButtonMouse.pressed ? "#3182ce" : (expandButtonMouse.containsMouse ? "#4a90c2" : "#5a6478")
            border.color: "#ffffff"
            border.width: 1
            radius: 4

            Text {
                anchors.centerIn: parent
                text: statusPanel.isExpanded ? "▼" : "▲"
                color: "#ffffff"
                font.pixelSize: 12
                font.bold: true
            }

            Behavior on color {
                ColorAnimation { duration: 150 }
            }

            Behavior on scale {
                NumberAnimation { duration: 100 }
            }

            MouseArea {
                id: expandButtonMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: {
                    console.log("Status panel", statusPanel.isExpanded ? "collapsing" : "expanding")
                    statusPanel.isExpanded = !statusPanel.isExpanded
                }

                onPressed: expandButton.scale = 0.9
                onReleased: expandButton.scale = 1.0
            }
        }

        // **PANEL TITLE**
        Text {
            id: panelTitle
            anchors.top: parent.top
            anchors.left: expandButton.right
            anchors.right: parent.right
            anchors.margins: 8
            text: statusPanel.isExpanded ? "Railway Control Panel - Extended" : "Controls"
            color: "#ffffff"
            font.pixelSize: statusPanel.isExpanded ? 14 : 12
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            height: 24
        }

        // **DYNAMIC GRID LAYOUT** for sections
        Grid {
            id: sectionsGrid
            anchors.top: panelTitle.bottom
            anchors.left: parent.left
            anchors.right: resizeHandleRight.left
            anchors.bottom: resizeHandleBottom.top
            anchors.margins: 12
            visible: statusPanel.isExpanded

            // **DYNAMIC LAYOUT CALCULATION**
            property real aspectRatio: width / height
            property int optimalColumns: {
                if (aspectRatio > 2.0) return 4      // Very wide: 4x1
                else if (aspectRatio > 1.2) return 2 // Wide: 2x2
                else return 1                        // Tall: 1x4
            }
            property int optimalRows: Math.ceil(4 / optimalColumns)

            columns: optimalColumns
            rows: optimalRows
            spacing: 8

            // **SECTION 1: SYSTEM INFO**
            Rectangle {
                width: (sectionsGrid.width - (sectionsGrid.columns - 1) * sectionsGrid.spacing) / sectionsGrid.columns
                height: (sectionsGrid.height - (sectionsGrid.rows - 1) * sectionsGrid.spacing) / sectionsGrid.rows
                color: "#374151"
                radius: 4
                border.color: "#4a5568"
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Text {
                        text: "System Info"
                        color: "#ffffff"
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }

                    Row {
                        width: parent.width
                        Text {
                            text: "Grid:"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            width: 50
                        }
                        Text {
                            text: cellSize + "px"
                            color: "#ffffff"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                    }

                    Row {
                        width: parent.width
                        Text {
                            text: "Layout:"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            width: 50
                        }
                        Text {
                            text: "360×200"
                            color: "#ffffff"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                    }

                    Row {
                        width: parent.width
                        Text {
                            text: "Tracks:"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            width: 50
                        }
                        Text {
                            text: StationData.trackSegments.length.toString()
                            color: "#38a169"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                    }
                }
            }

            // **SECTION 2: TRACK PADDING**
            Rectangle {
                width: (sectionsGrid.width - (sectionsGrid.columns - 1) * sectionsGrid.spacing) / sectionsGrid.columns
                height: (sectionsGrid.height - (sectionsGrid.rows - 1) * sectionsGrid.spacing) / sectionsGrid.rows
                color: "#374151"
                radius: 4
                border.color: "#4a5568"
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4  // Reduced spacing to prevent overlap

                    // **SECTION HEADER WITH TOGGLE DOT**
                    Item {
                        width: parent.width
                        height: 16  // Fixed height to prevent overlap

                        Text {
                            text: "Track Padding"
                            color: "#ffffff"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // **TOGGLE DOT** - Toggles between 0 and 1
                        Rectangle {
                            width: 14
                            height: 14
                            radius: 7
                            color: toggleMouse.pressed ? "#a0aec0" : "#ffffff"
                            border.color: "#666666"
                            border.width: 1
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter

                            // **TOGGLE STATE INDICATOR**
                            Text {
                                anchors.centerIn: parent
                                text: {
                                    // Show current toggle state (0 if both are 0, 1 if either is non-zero)
                                    return (stationLayout.globalRowPadding === 0 && stationLayout.globalColPadding === 0) ? "1" : "0"
                                }
                                color: "#000000"
                                font.pixelSize: 8
                                font.weight: Font.Bold
                            }

                            MouseArea {
                                id: toggleMouse
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true

                                onClicked: {
                                    // **TOGGLE LOGIC**: Switch between 0 and 1
                                    if (stationLayout.globalRowPadding === 0 && stationLayout.globalColPadding === 0) {
                                        // Currently 0 → Set to 1
                                        stationLayout.globalRowPadding = 1
                                        stationLayout.globalColPadding = 1
                                        console.log("Track padding toggled to 1,1")
                                    } else {
                                        // Currently non-zero → Set to 0
                                        stationLayout.globalRowPadding = 0
                                        stationLayout.globalColPadding = 0
                                        console.log("Track padding toggled to 0,0")
                                    }
                                }
                            }

                            // **HOVER EFFECT**
                            Behavior on color {
                                ColorAnimation { duration: 150 }
                            }

                            // **SCALE ANIMATION** on click
                            Behavior on scale {
                                NumberAnimation { duration: 100 }
                            }

                            scale: toggleMouse.pressed ? 0.9 : 1.0
                        }
                    }

                    // **ROW PADDING INPUT**
                    Row {
                        width: parent.width
                        height: 18  // Fixed height
                        spacing: 0

                        Text {
                            text: "Row:"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            width: 40
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Rectangle {
                            width: 45
                            height: 16  // Slightly smaller to fit better
                            color: "#4a5568"
                            border.color: rowInput.activeFocus ? "#3182ce" : "#666666"
                            border.width: 1
                            radius: 2
                            anchors.verticalCenter: parent.verticalCenter

                            TextInput {
                                id: rowInput
                                anchors.fill: parent
                                anchors.margins: 3
                                text: stationLayout.globalRowPadding.toString()
                                color: "#ffffff"
                                font.pixelSize: 8  // Smaller font to fit
                                horizontalAlignment: TextInput.AlignHCenter
                                verticalAlignment: TextInput.AlignVCenter
                                selectByMouse: true

                                onEditingFinished: {
                                    let value = parseFloat(text)
                                    if (!isNaN(value) && value >= 0 && value <= 10) {
                                        stationLayout.globalRowPadding = value
                                    } else {
                                        text = stationLayout.globalRowPadding.toString()
                                    }
                                }
                            }

                            Behavior on border.color {
                                ColorAnimation { duration: 150 }
                            }
                        }

                        Text {
                            text: " cells"
                            color: "#a0aec0"
                            font.pixelSize: 8
                            anchors.verticalCenter: parent.verticalCenter
                            leftPadding: 4
                        }
                    }

                    // **COLUMN PADDING INPUT**
                    Row {
                        width: parent.width
                        height: 18  // Fixed height
                        spacing: 0

                        Text {
                            text: "Col:"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            width: 40
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Rectangle {
                            width: 45
                            height: 16  // Slightly smaller to fit better
                            color: "#4a5568"
                            border.color: colInput.activeFocus ? "#3182ce" : "#666666"
                            border.width: 1
                            radius: 2
                            anchors.verticalCenter: parent.verticalCenter

                            TextInput {
                                id: colInput
                                anchors.fill: parent
                                anchors.margins: 3
                                text: stationLayout.globalColPadding.toString()
                                color: "#ffffff"
                                font.pixelSize: 8  // Smaller font to fit
                                horizontalAlignment: TextInput.AlignHCenter
                                verticalAlignment: TextInput.AlignVCenter
                                selectByMouse: true

                                onEditingFinished: {
                                    let value = parseFloat(text)
                                    if (!isNaN(value) && value >= 0 && value <= 10) {
                                        stationLayout.globalColPadding = value
                                    } else {
                                        text = stationLayout.globalColPadding.toString()
                                    }
                                }
                            }

                            Behavior on border.color {
                                ColorAnimation { duration: 150 }
                            }
                        }

                        Text {
                            text: " cells"
                            color: "#a0aec0"
                            font.pixelSize: 8
                            anchors.verticalCenter: parent.verticalCenter
                            leftPadding: 4
                        }
                    }

                    // **PADDING PREVIEW** (optional - shows current state)
                    Row {
                        width: parent.width
                        height: 12
                        visible: stationLayout.globalRowPadding > 0 || stationLayout.globalColPadding > 0

                        Text {
                            text: "Gap: " + (stationLayout.globalRowPadding * cellSize) + "×" + (stationLayout.globalColPadding * cellSize) + "px"
                            color: "#d69e2e"
                            font.pixelSize: 7
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // **SECTION 3: VIEW CONTROLS**
            Rectangle {
                width: (sectionsGrid.width - (sectionsGrid.columns - 1) * sectionsGrid.spacing) / sectionsGrid.columns
                height: (sectionsGrid.height - (sectionsGrid.rows - 1) * sectionsGrid.spacing) / sectionsGrid.rows
                color: "#374151"
                radius: 4
                border.color: "#4a5568"
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Text {
                        text: "View Controls"
                        color: "#ffffff"
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }

                    Rectangle {
                        width: parent.width - 10
                        height: 20
                        color: showGrid ? "#3182ce" : "#4a5568"
                        radius: 3

                        Text {
                            anchors.centerIn: parent
                            text: showGrid ? "Hide Grid" : "Show Grid"
                            color: "#ffffff"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: stationLayout.showGrid = !stationLayout.showGrid
                        }
                    }

                    Rectangle {
                        width: parent.width - 10
                        height: 18
                        color: "#4a5568"
                        border.color: "#666666"
                        border.width: 1
                        radius: 2

                        Text {
                            anchors.centerIn: parent
                            text: Math.round(canvas.gridOpacity * 100) + "%"
                            color: "#ffffff"
                            font.pixelSize: 9
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (canvas.gridOpacity <= 0.2) canvas.gridOpacity = 0.6
                                else if (canvas.gridOpacity <= 0.6) canvas.gridOpacity = 0.9
                                else canvas.gridOpacity = 0.2
                            }
                        }
                    }
                }
            }

            // **SECTION 4: ADVANCED CONTROLS**
            Rectangle {
                width: (sectionsGrid.width - (sectionsGrid.columns - 1) * sectionsGrid.spacing) / sectionsGrid.columns
                height: (sectionsGrid.height - (sectionsGrid.rows - 1) * sectionsGrid.spacing) / sectionsGrid.rows
                color: "#374151"
                radius: 4
                border.color: "#4a5568"
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Text {
                        text: "Advanced"
                        color: "#ffffff"
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }

                    Row {
                        width: parent.width
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: dbManager ? "#38a169" : "#ef4444"
                        }
                        Text {
                            text: dbManager ? "Connected" : "Offline"
                            color: dbManager ? "#38a169" : "#ef4444"
                            font.pixelSize: 9
                            leftPadding: 6
                        }
                    }

                    Row {
                        width: parent.width
                        Text {
                            text: "Signals:"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            width: 45
                        }
                        Text {
                            text: StationData.signalPositions.length.toString()
                            color: "#ffffff"
                            font.pixelSize: 9
                        }
                    }

                    Row {
                        width: parent.width
                        Text {
                            text: "Gates:"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            width: 45
                        }
                        Text {
                            text: StationData.levelCrossings.length.toString()
                            color: "#ffffff"
                            font.pixelSize: 9
                        }
                    }
                }
            }
        }

        // **RESIZE HANDLE - RIGHT EDGE**
        Rectangle {
            id: resizeHandleRight
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: 6
            color: "transparent"
            visible: statusPanel.isExpanded

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeHorCursor
                hoverEnabled: true

                property bool dragging: false
                property real startX: 0

                onPressed: {
                    dragging = true
                    startX = mouse.x
                }

                onPositionChanged: {
                    if (dragging) {
                        let newWidth = statusPanel.expandedWidth + (startX - mouse.x)
                        statusPanel.expandedWidth = Math.max(statusPanel.minWidth, Math.min(statusPanel.maxWidth, newWidth))
                    }
                }

                onReleased: {
                    dragging = false
                }
            }

            // **VISUAL INDICATOR**
            Rectangle {
                anchors.centerIn: parent
                width: 2
                height: parent.height * 0.3
                color: parent.children[0].containsMouse ? "#3182ce" : "#666666"
                radius: 1
            }
        }

        // **RESIZE HANDLE - BOTTOM EDGE**
        Rectangle {
            id: resizeHandleBottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 6
            color: "transparent"
            visible: statusPanel.isExpanded

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeVerCursor
                hoverEnabled: true

                property bool dragging: false
                property real startY: 0

                onPressed: {
                    dragging = true
                    startY = mouse.y
                }

                onPositionChanged: {
                    if (dragging) {
                        let newHeight = statusPanel.expandedHeight + (startY - mouse.y)
                        statusPanel.expandedHeight = Math.max(statusPanel.minHeight, Math.min(statusPanel.maxHeight, newHeight))
                    }
                }

                onReleased: {
                    dragging = false
                }
            }

            // **VISUAL INDICATOR**
            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.3
                height: 2
                color: parent.children[0].containsMouse ? "#3182ce" : "#666666"
                radius: 1
            }
        }

        // **RESIZE HANDLE - CORNER (BOTH DIRECTIONS)**
        Rectangle {
            id: resizeHandleCorner
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 12
            height: 12
            color: "transparent"
            visible: statusPanel.isExpanded

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SizeFDiagCursor
                hoverEnabled: true

                property bool dragging: false
                property real startX: 0
                property real startY: 0

                onPressed: {
                    dragging = true
                    startX = mouse.x
                    startY = mouse.y
                }

                onPositionChanged: {
                    if (dragging) {
                        let newWidth = statusPanel.expandedWidth + (startX - mouse.x)
                        let newHeight = statusPanel.expandedHeight + (startY - mouse.y)
                        statusPanel.expandedWidth = Math.max(statusPanel.minWidth, Math.min(statusPanel.maxWidth, newWidth))
                        statusPanel.expandedHeight = Math.max(statusPanel.minHeight, Math.min(statusPanel.maxHeight, newHeight))
                    }
                }

                onReleased: {
                    dragging = false
                }
            }

            // **CORNER GRIP LINES**
            Column {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.margins: 2
                spacing: 1

                Repeater {
                    model: 3
                    Rectangle {
                        width: 8 - (index * 2)
                        height: 1
                        color: parent.parent.children[0].containsMouse ? "#3182ce" : "#666666"
                    }
                }
            }
        }

        // **LAYOUT INFO** (top-right corner, for debugging)
        Text {
            anchors.top: parent.top
            anchors.right: resizeHandleRight.left
            anchors.margins: 4
            text: sectionsGrid.columns + "×" + sectionsGrid.rows
            color: "#666666"
            font.pixelSize: 8
            visible: statusPanel.isExpanded
        }
    }
}




