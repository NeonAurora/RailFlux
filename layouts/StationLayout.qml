import QtQuick
import "../components"
import "../data/StationData.js" as StationData

Rectangle {
    id: stationLayout
    color: "#1e1e1e"

    property var dbManager
    property int cellSize: Math.floor(width / 320)
    property bool showGrid: true
    property int signalRefreshTrigger: 0

    property var appStartTime: new Date()  // Capture when layout loads
    property string appUptime: "00:00:00"  // Formatted uptime string

    // Basic click handlers
    function handleTrackClick(segmentId, currentState) {
        console.log("Track segment clicked:", segmentId, "Currently occupied:", currentState)
    }

    function updateDisplay() {
        console.log("Station display updated from database")
    }

    function handleOuterSignalClick(signalId, currentAspect) {
        // Update the JavaScript data
        var signal = StationData.getOuterSignalById(signalId)
        if (signal) {
            var currentIndex = signal.possibleAspects.indexOf(currentAspect)
            var nextIndex = (currentIndex + 1) % signal.possibleAspects.length
            signal.currentAspect = signal.possibleAspects[nextIndex]

            // ✅ Trigger refresh: This causes QML to re-evaluate all bindings that depend on this property
            signalRefreshTrigger = signalRefreshTrigger + 1
        }
    }

    function handleHomeSignalClick(signalId, currentAspect) {
        // Update the JavaScript data
        var signal = StationData.getHomeSignalById(signalId)
        if (signal) {
            var currentIndex = signal.possibleAspects.indexOf(currentAspect)
            var nextIndex = (currentIndex + 1) % signal.possibleAspects.length
            signal.currentAspect = signal.possibleAspects[nextIndex]

            console.log("Changed home signal", signalId, "to", signal.currentAspect)

            // Trigger refresh
            signalRefreshTrigger = signalRefreshTrigger + 1
        }
    }

    function handleStarterSignalClick(signalId, currentAspect) {
        // Update the JavaScript data
        var signal = StationData.getStarterSignalById(signalId)
        if (signal) {
            var currentIndex = signal.possibleAspects.indexOf(currentAspect)
            var nextIndex = (currentIndex + 1) % signal.possibleAspects.length
            signal.currentAspect = signal.possibleAspects[nextIndex]

            console.log("Changed starter signal", signalId, "to", signal.currentAspect)

            // Trigger refresh
            signalRefreshTrigger = signalRefreshTrigger + 1
        }
    }

    function handlePointMachineClick(machineId, currentPosition) {
        console.log("Point machine click handler:", machineId, "Current:", currentPosition)

        // Determine target position
        var targetPosition = (currentPosition === "NORMAL") ? "REVERSE" : "NORMAL"

        // ✅ FIXED: Use the updated function that returns result object
        var result = StationData.operatePointMachine(machineId, targetPosition)

        if (result.success) {
            console.log("Point machine operation initiated:", machineId, "→", targetPosition)

            // Trigger immediate UI refresh for transition state
            signalRefreshTrigger = signalRefreshTrigger + 1

            // ✅ FIXED: Use QML Timer instead of setTimeout
            if (result.transitionTime > 0) {
                var timer = Qt.createQmlObject(`
                    import QtQuick 2.0
                    Timer {
                        interval: ${result.transitionTime}
                        running: true
                        repeat: false
                    }
                `, stationLayout)

                timer.triggered.connect(function() {
                    // Complete the operation
                    StationData.completePointMachineOperation(machineId, targetPosition)

                    // Trigger final UI refresh
                    signalRefreshTrigger = signalRefreshTrigger + 1
                    console.log("Point machine transition completed:", machineId)

                    // Clean up timer
                    timer.destroy()
                })
            }
        } else {
            console.warn("Point machine operation failed:", result.reason)
            // Could show user notification here
        }
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

                isOccupied: {
                    if (!stationLayout.dbManager) return modelData.occupied
                    return modelData.occupied
                }

                onTrackClicked: stationLayout.handleTrackClick(segmentId, currentState)
            }
        }

        // **LAYER 2: Point Machines** (middle layer)
        Repeater {
            model: StationData.pointMachines
            PointMachine {
                machineId: modelData.id
                position: {
                    stationLayout.signalRefreshTrigger
                    var pm = StationData.getPointMachineById(modelData.id)
                    return pm ? pm.position : "NORMAL"
                }
                operatingStatus: {
                    stationLayout.signalRefreshTrigger
                    var pm = StationData.getPointMachineById(modelData.id)
                    return pm ? pm.operatingStatus : "CONNECTED"
                }
                junctionPoint: modelData.junctionPoint
                rootTrack: modelData.rootTrack
                normalTrack: modelData.normalTrack
                reverseTrack: modelData.reverseTrack
                cellSize: stationLayout.cellSize
            }
        }

        // Outer Signals Rendering Block
        Repeater {
            model: StationData.getAllOuterSignals()

            OuterSignal {
                x: modelData.col * stationLayout.cellSize
                y: modelData.row * stationLayout.cellSize
                signalId: modelData.id
                signalName: modelData.name
                currentAspect: {
                    stationLayout.signalRefreshTrigger  // Depend on this value
                    var signal = StationData.getOuterSignalById(signalId)
                    return signal ? signal.currentAspect : "RED"
                }
                direction: modelData.direction
                isActive: modelData.isActive
                cellSize: stationLayout.cellSize

                onSignalClicked: {
                    console.log("Outer signal control:", signalId, currentAspect)
                    // TODO: Add signal control logic here
                    stationLayout.handleOuterSignalClick(signalId, currentAspect)
                }
            }
        }

        // Basic signals


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

        Repeater {
            model: StationData.getAllHomeSignals()

            HomeSignal {
                x: modelData.col * stationLayout.cellSize
                y: modelData.row * stationLayout.cellSize
                signalId: modelData.id
                signalName: modelData.name
                currentAspect: {
                    stationLayout.signalRefreshTrigger  // Depend on refresh trigger
                    var signal = StationData.getHomeSignalById(signalId)
                    return signal ? signal.currentAspect : "RED"
                }
                direction: modelData.direction
                isActive: modelData.isActive
                cellSize: stationLayout.cellSize

                onSignalClicked: {
                    console.log("Home signal control:", signalId, currentAspect)
                    stationLayout.handleHomeSignalClick(signalId, currentAspect)
                }
            }
        }

        Repeater {
            model: StationData.getAllStarterSignals()

            StarterSignal {
                x: modelData.col * stationLayout.cellSize
                y: modelData.row * stationLayout.cellSize
                signalId: modelData.id
                signalName: modelData.name
                currentAspect: {
                    stationLayout.signalRefreshTrigger  // Depend on refresh trigger
                    var signal = StationData.getStarterSignalById(signalId)
                    return signal ? signal.currentAspect : "RED"
                }
                aspectCount: modelData.aspectCount
                direction: modelData.direction
                isActive: modelData.isActive
                cellSize: stationLayout.cellSize

                onSignalClicked: {
                    console.log("Starter signal control:", signalId, currentAspect)
                    stationLayout.handleStarterSignalClick(signalId, currentAspect)
                }
            }
        }

        Repeater {
            model: StationData.getAllPointMachines()

            PointMachine {
                machineId: modelData.id
                position: {
                    stationLayout.signalRefreshTrigger  // Trigger refresh
                    var pm = StationData.getPointMachineById(modelData.id)
                    return pm ? pm.position : "NORMAL"
                }
                operatingStatus: {
                    stationLayout.signalRefreshTrigger  // Trigger refresh
                    var pm = StationData.getPointMachineById(modelData.id)
                    return pm ? pm.operatingStatus : "CONNECTED"
                }
                junctionPoint: modelData.junctionPoint
                rootTrack: modelData.rootTrack
                normalTrack: modelData.normalTrack
                reverseTrack: modelData.reverseTrack
                cellSize: stationLayout.cellSize

                onPointMachineClicked: function(machineId, currentPosition) {
                    console.log("Point machine operation requested:", machineId, currentPosition)
                    stationLayout.handlePointMachineClick(machineId, currentPosition)
                }
            }
        }
    }

    Timer {
        id: uptimeTimer
        interval: 1000  // Update every second
        running: true   // Start immediately
        repeat: true    // Keep running

        onTriggered: {
            var currentTime = new Date()
            var uptimeMs = currentTime.getTime() - stationLayout.appStartTime.getTime()

            // Convert milliseconds to hours:minutes:seconds
            var totalSeconds = Math.floor(uptimeMs / 1000)
            var hours = Math.floor(totalSeconds / 3600)
            var minutes = Math.floor((totalSeconds % 3600) / 60)
            var seconds = totalSeconds % 60

            // Format with leading zeros
            var hoursStr = hours.toString().padStart(2, '0')
            var minutesStr = minutes.toString().padStart(2, '0')
            var secondsStr = seconds.toString().padStart(2, '0')

            // Update the uptime display
            stationLayout.appUptime = hoursStr + ":" + minutesStr + ":" + secondsStr
        }
    }

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
                if (aspectRatio > 2.0) return 3      // Very wide: 4x1
                else if (aspectRatio > 1.0) return 2 // Wide: 2x2
                else return 1                        // Tall: 1x4
            }
            property int optimalRows: Math.ceil(3 / optimalColumns)

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

            // **SECTION 2: VIEW CONTROLS**
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

            // **SECTION 4: ADVANCED CONTROLS** - Updated version
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

                    // **DATABASE CONNECTION STATUS**
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

                    // **APP UPTIME DISPLAY** - Simplified property access
                    Row {
                        width: parent.width
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: "#3182ce"  // Blue indicator
                        }
                        Text {
                            text: "Uptime: " + stationLayout.appUptime  // ✅ Direct access to stationLayout
                            color: "#a0aec0"
                            font.pixelSize: 9
                            leftPadding: 6
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




