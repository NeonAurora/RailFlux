import QtQuick
import "../components"
// ✅ REMOVED: import "../data/StationData.js" as StationData

Rectangle {
    id: stationLayout
    color: "#1e1e1e"

    property var dbManager
    property int cellSize: Math.floor(width / 320)
    property bool showGrid: true

    // ✅ NEW: Data properties from database (replaces StationData.js)
    property var trackSegmentsModel: []
    property var outerSignalsModel: []
    property var homeSignalsModel: []
    property var starterSignalsModel: []
    property var advanceStarterSignalsModel: []
    property var pointMachinesModel: []
    property var textLabelsModel: []

    // App timing properties
    property var appStartTime: new Date()
    property string appUptime: "00:00:00"

    // ✅ NEW: Data refresh functions (replaces signalRefreshTrigger)
    function refreshAllData() {
        if (!dbManager || !dbManager.isConnected) {
            console.log("Database not connected - cannot refresh data")
            return
        }

        console.log("Refreshing all station data from database")
        refreshTrackData()
        refreshSignalData()
        refreshPointMachineData()
        refreshTextLabelData()
    }

    function refreshTrackData() {
        if (!dbManager || !dbManager.isConnected) return

        console.log("Refreshing track segments from database")
        trackSegmentsModel = dbManager.getTrackSegmentsList()
        console.log("Loaded", trackSegmentsModel.length, "track segments")
    }

    function refreshSignalData() {
        if (!dbManager || !dbManager.isConnected) return

        console.log("Refreshing signals from database")
        var allSignals = dbManager.getAllSignalsList()

        // Filter signals by type
        outerSignalsModel = allSignals.filter(signal => signal.type === "OUTER")
        homeSignalsModel = allSignals.filter(signal => signal.type === "HOME")
        starterSignalsModel = allSignals.filter(signal => signal.type === "STARTER")
        advanceStarterSignalsModel = allSignals.filter(signal => signal.type === "ADVANCED_STARTER")

        console.log("Loaded signals - Outer:", outerSignalsModel.length,
                   "Home:", homeSignalsModel.length,
                   "Starter:", starterSignalsModel.length,
                   "Advanced:", advanceStarterSignalsModel.length)
    }

    function refreshPointMachineData() {
        if (!dbManager || !dbManager.isConnected) return

        console.log("Refreshing point machines from database")
        pointMachinesModel = dbManager.getAllPointMachinesList()
        console.log("Loaded", pointMachinesModel.length, "point machines")
    }

    function refreshTextLabelData() {
        if (!dbManager || !dbManager.isConnected) return

        console.log("Refreshing text labels from database")
        textLabelsModel = dbManager.getTextLabelsList()
        console.log("Loaded", textLabelsModel.length, "text labels")
    }

    // ✅ UPDATED: Signal handlers now update database instead of StationData.js
    function handleTrackClick(segmentId, currentState) {
        console.log("Track segment clicked:", segmentId, "Currently occupied:", currentState)

        if (!dbManager || !dbManager.isConnected) {
            console.warn("Database not connected - cannot update track state")
            return
        }

        // Toggle occupancy state in database
        var newState = !currentState
        console.log("Updating track", segmentId, "occupancy to:", newState)

        var success = dbManager.updateTrackOccupancy(segmentId, newState)
        if (success) {
            console.log("Track occupancy updated successfully")
            // Data will be refreshed automatically via database signals
        } else {
            console.error("Failed to update track occupancy")
        }
    }

    function updateDisplay() {
        console.log("Station display update requested")
        refreshAllData()
    }

    function handleOuterSignalClick(signalId, currentAspect) {
        console.log("Outer signal control:", signalId, "Current aspect:", currentAspect)

        if (!dbManager || !dbManager.isConnected) {
            console.warn("Database not connected - cannot update signal")
            return
        }

        // Get signal data from database to find possible aspects
        var signalData = dbManager.getSignalById(signalId)
        if (!signalData || !signalData.possibleAspects) {
            console.error("Could not get signal data for", signalId)
            return
        }

        // Calculate next aspect
        var possibleAspects = signalData.possibleAspects
        var currentIndex = possibleAspects.indexOf(currentAspect)
        var nextIndex = (currentIndex + 1) % possibleAspects.length
        var nextAspect = possibleAspects[nextIndex]

        console.log("Changing outer signal", signalId, "from", currentAspect, "to", nextAspect)

        // Update in database
        var success = dbManager.updateSignalAspect(signalId, nextAspect)
        if (success) {
            console.log("Outer signal aspect updated successfully")
        } else {
            console.error("Failed to update outer signal aspect")
        }
    }

    function handleHomeSignalClick(signalId, currentAspect) {
        console.log("Home signal control:", signalId, "Current aspect:", currentAspect)

        if (!dbManager || !dbManager.isConnected) {
            console.warn("Database not connected - cannot update signal")
            return
        }

        var signalData = dbManager.getSignalById(signalId)
        if (!signalData || !signalData.possibleAspects) {
            console.error("Could not get signal data for", signalId)
            return
        }

        var possibleAspects = signalData.possibleAspects
        var currentIndex = possibleAspects.indexOf(currentAspect)
        var nextIndex = (currentIndex + 1) % possibleAspects.length
        var nextAspect = possibleAspects[nextIndex]

        console.log("Changing home signal", signalId, "from", currentAspect, "to", nextAspect)

        var success = dbManager.updateSignalAspect(signalId, nextAspect)
        if (success) {
            console.log("Home signal aspect updated successfully")
        } else {
            console.error("Failed to update home signal aspect")
        }
    }

    function handleStarterSignalClick(signalId, currentAspect) {
        console.log("Starter signal control:", signalId, "Current aspect:", currentAspect)

        if (!dbManager || !dbManager.isConnected) {
            console.warn("Database not connected - cannot update signal")
            return
        }

        var signalData = dbManager.getSignalById(signalId)
        if (!signalData || !signalData.possibleAspects) {
            console.error("Could not get signal data for", signalId)
            return
        }

        var possibleAspects = signalData.possibleAspects
        var currentIndex = possibleAspects.indexOf(currentAspect)
        var nextIndex = (currentIndex + 1) % possibleAspects.length
        var nextAspect = possibleAspects[nextIndex]

        console.log("Changing starter signal", signalId, "from", currentAspect, "to", nextAspect)

        var success = dbManager.updateSignalAspect(signalId, nextAspect)
        if (success) {
            console.log("Starter signal aspect updated successfully")
        } else {
            console.error("Failed to update starter signal aspect")
        }
    }

    function handlePointMachineClick(machineId, currentPosition) {
        console.log("Point machine operation requested:", machineId, "Current position:", currentPosition)

        if (!dbManager || !dbManager.isConnected) {
            console.warn("Database not connected - cannot operate point machine")
            return
        }

        // Determine target position
        var targetPosition = (currentPosition === "NORMAL") ? "REVERSE" : "NORMAL"

        console.log("Operating point machine", machineId, "from", currentPosition, "to", targetPosition)

        // Update in database - this will handle the transition logic
        var success = dbManager.updatePointMachinePosition(machineId, targetPosition)
        if (success) {
            console.log("Point machine operation initiated successfully")
            // The database will handle the transition timing and state updates
            // UI will be updated automatically via polling or notifications
        } else {
            console.error("Failed to operate point machine")
        }
    }

    function handleAdvanceStarterSignalClick(signalId, currentAspect) {
        console.log("Advanced starter signal control:", signalId, "Current aspect:", currentAspect)

        if (!dbManager || !dbManager.isConnected) {
            console.warn("Database not connected - cannot update signal")
            return
        }

        var signalData = dbManager.getSignalById(signalId)
        if (!signalData || !signalData.possibleAspects) {
            console.error("Could not get signal data for", signalId)
            return
        }

        var possibleAspects = signalData.possibleAspects
        var currentIndex = possibleAspects.indexOf(currentAspect)
        var nextIndex = (currentIndex + 1) % possibleAspects.length
        var nextAspect = possibleAspects[nextIndex]

        console.log("Changing advanced starter signal", signalId, "from", currentAspect, "to", nextAspect)

        var success = dbManager.updateSignalAspect(signalId, nextAspect)
        if (success) {
            console.log("Advanced starter signal aspect updated successfully")
        } else {
            console.error("Failed to update advanced starter signal aspect")
        }
    }

    // ✅ NEW: Initialize data when component loads or database connects
    Component.onCompleted: {
        console.log("StationLayout: Component completed")
        if (dbManager && dbManager.isConnected) {
            refreshAllData()
        } else {
            console.log("Database not ready yet - will load data when connected")
        }
    }

    // ✅ NEW: Watch for database connection and data changes
    Connections {
        target: dbManager

        function onConnectionStateChanged(isConnected) {
            console.log("StationLayout: Database connection state changed:", isConnected)
            if (isConnected) {
                refreshAllData()
            } else {
                console.log("Database disconnected - clearing data models")
                trackSegmentsModel = []
                outerSignalsModel = []
                homeSignalsModel = []
                starterSignalsModel = []
                advanceStarterSignalsModel = []
                pointMachinesModel = []
                textLabelsModel = []
            }
        }

        // ✅ Handle database polling updates
        function onDataUpdated() {
            console.log("StationLayout: Database data updated (polling)")
            refreshAllData()
        }

        // ✅ Handle real-time notifications (if available)
        function onTrackSegmentUpdated(segmentId) {
            console.log("StationLayout: Track segment updated:", segmentId)
            refreshTrackData()
        }

        function onSignalUpdated(signalId) {
            console.log("StationLayout: Signal updated:", signalId)
            refreshSignalData()
        }

        function onPointMachineUpdated(machineId) {
            console.log("StationLayout: Point machine updated:", machineId)
            refreshPointMachineData()
        }

        // ✅ Handle batch updates
        function onTrackSegmentsChanged() {
            console.log("StationLayout: Track segments changed")
            refreshTrackData()
        }

        function onSignalsChanged() {
            console.log("StationLayout: Signals changed")
            refreshSignalData()
        }

        function onPointMachinesChanged() {
            console.log("StationLayout: Point machines changed")
            refreshPointMachineData()
        }

        function onTextLabelsChanged() {
            console.log("StationLayout: Text labels changed")
            refreshTextLabelData()
        }
    }

    // Main grid canvas
    GridCanvas {
        id: canvas
        anchors.fill: parent
        gridSize: stationLayout.cellSize
        showGrid: stationLayout.showGrid

        // ✅ UPDATED: Track segments from database
        Repeater {
            model: trackSegmentsModel

            TrackSegment {
                segmentId: modelData.id
                startRow: modelData.startRow
                startCol: modelData.startCol
                endRow: modelData.endRow
                endCol: modelData.endCol
                cellSize: stationLayout.cellSize
                isOccupied: modelData.occupied
                isAssigned: modelData.assigned
                onTrackClicked: stationLayout.handleTrackClick(segmentId, isOccupied)
            }
        }

        // ✅ UPDATED: Point machines from database
        Repeater {
            model: pointMachinesModel

            PointMachine {
                machineId: modelData.id
                position: modelData.position
                operatingStatus: modelData.operatingStatus
                junctionPoint: modelData.junctionPoint
                rootTrack: modelData.rootTrack
                normalTrack: modelData.normalTrack
                reverseTrack: modelData.reverseTrack
                cellSize: stationLayout.cellSize

                onPointMachineClicked: function(machineId, currentPosition) {
                    stationLayout.handlePointMachineClick(machineId, currentPosition)
                }
            }
        }

        // ✅ UPDATED: Outer signals from database
        Repeater {
            model: outerSignalsModel

            OuterSignal {
                x: modelData.col * stationLayout.cellSize
                y: modelData.row * stationLayout.cellSize
                signalId: modelData.id
                signalName: modelData.name
                currentAspect: modelData.currentAspect
                direction: modelData.direction
                isActive: modelData.isActive
                cellSize: stationLayout.cellSize
                onSignalClicked: stationLayout.handleOuterSignalClick(signalId, currentAspect)
            }
        }

        // ✅ UPDATED: Home signals from database
        Repeater {
            model: homeSignalsModel

            HomeSignal {
                x: modelData.col * stationLayout.cellSize
                y: modelData.row * stationLayout.cellSize
                signalId: modelData.id
                signalName: modelData.name
                currentAspect: modelData.currentAspect
                direction: modelData.direction
                isActive: modelData.isActive
                cellSize: stationLayout.cellSize
                onSignalClicked: stationLayout.handleHomeSignalClick(signalId, currentAspect)
            }
        }

        // ✅ UPDATED: Starter signals from database
        Repeater {
            model: starterSignalsModel

            StarterSignal {
                x: modelData.col * stationLayout.cellSize
                y: modelData.row * stationLayout.cellSize
                signalId: modelData.id
                signalName: modelData.name
                currentAspect: modelData.currentAspect
                aspectCount: modelData.aspectCount
                direction: modelData.direction
                isActive: modelData.isActive
                cellSize: stationLayout.cellSize
                onSignalClicked: stationLayout.handleStarterSignalClick(signalId, currentAspect)
            }
        }

        // ✅ UPDATED: Advanced starter signals from database
        Repeater {
            model: advanceStarterSignalsModel

            AdvanceStarterSignal {
                x: modelData.col * stationLayout.cellSize
                y: modelData.row * stationLayout.cellSize
                signalId: modelData.id
                signalName: modelData.name
                currentAspect: modelData.currentAspect
                direction: modelData.direction
                isActive: modelData.isActive
                cellSize: stationLayout.cellSize
                onSignalClicked: stationLayout.handleAdvanceStarterSignalClick(signalId, currentAspect)
            }
        }

        // ✅ UPDATED: Text labels from database
        Repeater {
            model: textLabelsModel

            Text {
                x: modelData.col * stationLayout.cellSize
                y: modelData.row * stationLayout.cellSize
                text: modelData.text
                color: modelData.color || "#ffffff"
                font.pixelSize: modelData.fontSize || 12
                font.family: modelData.fontFamily || "Arial"
                visible: modelData.isVisible !== false
            }
        }
    }

    // Uptime timer (unchanged)
    Timer {
        id: uptimeTimer
        interval: 1000
        running: true
        repeat: true

        onTriggered: {
            var currentTime = new Date()
            var uptimeMs = currentTime.getTime() - stationLayout.appStartTime.getTime()
            var totalSeconds = Math.floor(uptimeMs / 1000)
            var hours = Math.floor(totalSeconds / 3600)
            var minutes = Math.floor((totalSeconds % 3600) / 60)
            var seconds = totalSeconds % 60
            var hoursStr = hours.toString().padStart(2, '0')
            var minutesStr = minutes.toString().padStart(2, '0')
            var secondsStr = seconds.toString().padStart(2, '0')
            stationLayout.appUptime = hoursStr + ":" + minutesStr + ":" + secondsStr
        }
    }

    // ✅ ENHANCED: Status panel with database integration
    Rectangle {
        id: statusPanel
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 10

        property bool isExpanded: false
        property int collapsedWidth: 220
        property int collapsedHeight: 60
        property int expandedWidth: 400
        property int expandedHeight: 350

        width: isExpanded ? expandedWidth : collapsedWidth
        height: isExpanded ? expandedHeight : collapsedHeight

        color: "#2d3748"
        border.color: isExpanded ? "#3182ce" : "#4a5568"
        border.width: 2
        radius: 6

        Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutQuart } }
        Behavior on height { NumberAnimation { duration: 300; easing.type: Easing.OutQuart } }
        Behavior on border.color { ColorAnimation { duration: 300 } }

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

            MouseArea {
                id: expandButtonMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: statusPanel.isExpanded = !statusPanel.isExpanded
            }
        }

        Text {
            id: panelTitle
            anchors.top: parent.top
            anchors.left: expandButton.right
            anchors.right: parent.right
            anchors.margins: 8
            text: statusPanel.isExpanded ? "Railway Control Panel - Database Mode" : "Controls"
            color: "#ffffff"
            font.pixelSize: statusPanel.isExpanded ? 14 : 12
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
            height: 24
        }

        Grid {
            id: sectionsGrid
            anchors.top: panelTitle.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            visible: statusPanel.isExpanded

            columns: 2
            rows: 2
            spacing: 8

            // ✅ UPDATED: Database-aware system info
            Rectangle {
                width: (sectionsGrid.width - sectionsGrid.spacing) / 2
                height: (sectionsGrid.height - sectionsGrid.spacing) / 2
                color: "#374151"
                radius: 4
                border.color: "#4a5568"
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Text {
                        text: "Database Status"
                        color: "#ffffff"
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }

                    Row {
                        width: parent.width
                        Text {
                            text: "Connection:"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            width: 60
                        }
                        Text {
                            text: dbManager && dbManager.isConnected ? "Connected" : "Offline"
                            color: dbManager && dbManager.isConnected ? "#38a169" : "#ef4444"
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
                            width: 60
                        }
                        Text {
                            text: trackSegmentsModel.length.toString()
                            color: "#38a169"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                    }

                    Row {
                        width: parent.width
                        Text {
                            text: "Signals:"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            width: 60
                        }
                        Text {
                            text: (outerSignalsModel.length + homeSignalsModel.length +
                                  starterSignalsModel.length + advanceStarterSignalsModel.length).toString()
                            color: "#38a169"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                    }

                    Row {
                        width: parent.width
                        Text {
                            text: "Points:"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            width: 60
                        }
                        Text {
                            text: pointMachinesModel.length.toString()
                            color: "#38a169"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                    }
                }
            }

            // View controls section
            Rectangle {
                width: (sectionsGrid.width - sectionsGrid.spacing) / 2
                height: (sectionsGrid.height - sectionsGrid.spacing) / 2
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

                    // ✅ NEW: Manual refresh button
                    Rectangle {
                        width: parent.width - 10
                        height: 20
                        color: refreshMouse.pressed ? "#2c5aa0" : "#3182ce"
                        radius: 3

                        Text {
                            anchors.centerIn: parent
                            text: "🔄 Refresh Data"
                            color: "#ffffff"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            id: refreshMouse
                            anchors.fill: parent
                            onClicked: {
                                console.log("Manual data refresh requested")
                                stationLayout.refreshAllData()
                            }
                        }
                    }
                }
            }

            // Data source info section
            Rectangle {
                width: (sectionsGrid.width - sectionsGrid.spacing) / 2
                height: (sectionsGrid.height - sectionsGrid.spacing) / 2
                color: "#374151"
                radius: 4
                border.color: "#4a5568"
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Text {
                        text: "Data Source"
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
                            color: "#3182ce"
                        }
                        Text {
                            text: "PostgreSQL Database"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            leftPadding: 6
                        }
                    }

                    Row {
                        width: parent.width
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: "#38a169"
                        }
                        Text {
                            text: "Polling: 50s interval"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            leftPadding: 6
                        }
                    }

                    Row {
                        width: parent.width
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: "#f6ad55"
                        }
                        Text {
                            text: "Real-time updates"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            leftPadding: 6
                        }
                    }

                    Row {
                        width: parent.width
                        Text {
                            text: "Uptime:"
                            color: "#a0aec0"
                            font.pixelSize: 9
                            width: 40
                        }
                        Text {
                            text: stationLayout.appUptime
                            color: "#ffffff"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                    }
                }
            }

            // Database controls section
            Rectangle {
                width: (sectionsGrid.width - sectionsGrid.spacing) / 2
                height: (sectionsGrid.height - sectionsGrid.spacing) / 2
                color: "#374151"
                radius: 4
                border.color: "#4a5568"
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Text {
                        text: "Database Controls"
                        color: "#ffffff"
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }

                    Rectangle {
                        width: parent.width - 10
                        height: 18
                        color: testConnectionMouse.pressed ? "#2c5aa0" : "#3182ce"
                        radius: 3

                        Text {
                            anchors.centerIn: parent
                            text: "Test Connection"
                            color: "#ffffff"
                            font.pixelSize: 8
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            id: testConnectionMouse
                            anchors.fill: parent
                            onClicked: {
                                console.log("Testing database connection...")
                                if (globalDatabaseInitializer) {
                                    globalDatabaseInitializer.testConnection()
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width - 10
                        height: 20
                        color: resetButtonMouse.pressed ? "#c53030" : "#e53e3e"
                        radius: 3

                        Text {
                            anchors.centerIn: parent
                            text: "⚠️ Reset Database"
                            color: "#ffffff"
                            font.pixelSize: 8
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            id: resetButtonMouse
                            anchors.fill: parent
                            onClicked: {
                                console.log("Database reset requested from status panel")
                                // This should open the reset dialog in Main.qml
                                // You might need to emit a signal or call a parent function
                            }
                        }
                    }
                }
            }
        }
    }
}
