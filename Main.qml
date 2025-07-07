import QtQuick
import QtQuick.Controls
import RailFlux.Database
import "layouts" as Layouts

ApplicationWindow {
    id: mainWindow
    width: 1920
    height: 1000
    visible: true
    title: qsTr("RailFlux - Railway Control System")

    color: "#1a1a1a"

    // ✅ ENHANCED: Database Manager Connections
    Connections {
        target: globalDatabaseManager

        function onConnectionStateChanged(isConnected) {
            console.log("Database connection changed:", isConnected)
            connectionStatus.text = isConnected ? "Connected" : "Disconnected"
            connectionStatus.color = isConnected ? theme.successGreen : theme.dangerRed

            // ✅ NEW: Refresh all data when connection is established
            if (isConnected) {
                console.log("Database connected - refreshing all station data")
                stationLayout.refreshAllData()
            }
        }

        function onDataUpdated() {
            console.log("Database data updated - refreshing UI")
            stationLayout.refreshAllData()
        }

        // ✅ NEW: Handle specific entity updates for better performance
        function onSignalUpdated(signalId) {
            console.log("Signal updated:", signalId)
            stationLayout.refreshSignalData()
        }

        function onPointMachineUpdated(machineId) {
            console.log("Point machine updated:", machineId)
            stationLayout.refreshPointMachineData()
        }

        function onTrackSegmentUpdated(segmentId) {
            console.log("Track segment updated:", segmentId)
            stationLayout.refreshTrackData()
        }

        // ✅ NEW: Handle batch data changes
        function onTrackSegmentsChanged() {
            console.log("Track segments data changed")
            stationLayout.refreshTrackData()
        }

        function onSignalsChanged() {
            console.log("Signals data changed")
            stationLayout.refreshSignalData()
        }

        function onPointMachinesChanged() {
            console.log("Point machines data changed")
            stationLayout.refreshPointMachineData()
        }

        function onTextLabelsChanged() {
            console.log("Text labels data changed")
            stationLayout.refreshTextLabelData()
        }
    }

    // ✅ ENHANCED: Database Initializer Connections
    Connections {
        target: globalDatabaseInitializer

        function onResetCompleted(success, message) {
            console.log("Database reset completed:", success, message)
            resetResultDialog.success = success
            resetResultDialog.message = message
            resetResultDialog.open()

            // ✅ ENHANCED: Restart and refresh after successful reset
            if (success && globalDatabaseManager) {
                console.log("Database reset successful - reconnecting and refreshing data")

                // Give database a moment to settle
                reconnectTimer.start()
            }
        }

        function onConnectionTestCompleted(success, message) {
            console.log("Connection test completed:", success, message)
            connectionTestDialog.success = success
            connectionTestDialog.message = message
            connectionTestDialog.open()
        }

        // ✅ NEW: Handle reset progress updates
        function onProgressChanged() {
            // Progress updates are handled by property bindings in the dialog
            console.log("Database reset progress:", globalDatabaseInitializer.progress + "%")
        }
    }

    // ✅ NEW: Timer for database reconnection after reset
    Timer {
        id: reconnectTimer
        interval: 2000  // Wait 2 seconds for database to settle
        running: false
        repeat: false

        onTriggered: {
            console.log("Attempting to reconnect to database after reset")
            if (globalDatabaseManager) {
                globalDatabaseManager.connectToDatabase()
                globalDatabaseManager.startPolling()

                // Give connection time to establish, then refresh data
                Qt.callLater(function() {
                    if (globalDatabaseManager.isConnected) {
                        stationLayout.refreshAllData()
                    }
                })
            }
        }
    }

    // ✅ NEW: Application initialization
    Component.onCompleted: {
        console.log("RailFlux application starting up")

        // Initialize database connection if not already connected
        if (globalDatabaseManager && !globalDatabaseManager.isConnected) {
            console.log("Initializing database connection")
            globalDatabaseManager.connectToDatabase()
            globalDatabaseManager.startPolling()
        }

        // Initialize data once database is ready
        if (globalDatabaseManager && globalDatabaseManager.isConnected) {
            stationLayout.refreshAllData()
        }
    }

    QtObject {
        id: theme
        readonly property color darkBackground: "#1a1a1a"
        readonly property color controlBackground: "#2d3748"
        readonly property color accentBlue: "#3182ce"
        readonly property color successGreen: "#38a169"
        readonly property color warningYellow: "#d69e2e"
        readonly property color dangerRed: "#e53e3e"
        readonly property color textPrimary: "#ffffff"
        readonly property color textSecondary: "#a0aec0"
        readonly property color borderColor: "#4a5568"

        readonly property int spacingSmall: 8
        readonly property int spacingMedium: 16
        readonly property int spacingLarge: 24
    }

    // ✅ ENHANCED: Database Reset Confirmation Dialog
    Dialog {
        id: databaseResetDialog
        title: "⚠️ Database Reset Confirmation"
        modal: true
        anchors.centerIn: parent
        width: Math.min(600, parent.width * 0.8)
        height: Math.min(450, parent.height * 0.7)

        background: Rectangle {
            color: theme.controlBackground
            border.color: theme.dangerRed
            border.width: 2
            radius: 8
        }

        Column {
            anchors.fill: parent
            anchors.margins: theme.spacingLarge
            spacing: theme.spacingMedium

            Text {
                width: parent.width
                text: "⚠️ DESTRUCTIVE OPERATION WARNING"
                font.pixelSize: 18
                font.weight: Font.Bold
                color: theme.dangerRed
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Rectangle {
                width: parent.width
                height: 2
                color: theme.dangerRed
            }

            Text {
                width: parent.width
                text: "This operation will completely rebuild the database with fresh data:"
                font.pixelSize: 14
                font.weight: Font.Bold
                color: theme.textPrimary
                wrapMode: Text.Wrap
            }

            Column {
                width: parent.width
                spacing: theme.spacingSmall

                Text {
                    text: "• DROP all existing railway control schemas and tables"
                    font.pixelSize: 12
                    color: theme.textSecondary
                }
                Text {
                    text: "• DELETE all current track, signal, and point machine states"
                    font.pixelSize: 12
                    color: theme.textSecondary
                }
                Text {
                    text: "• ERASE all operational history and audit logs"
                    font.pixelSize: 12
                    color: theme.textSecondary
                }
                Text {
                    text: "• RECREATE database with default station configuration"
                    font.pixelSize: 12
                    color: theme.textSecondary
                }
                Text {
                    text: "• RESTORE signals to default RED state"
                    font.pixelSize: 12
                    color: theme.textSecondary
                }
                Text {
                    text: "• RESET point machines to configured positions"
                    font.pixelSize: 12
                    color: theme.textSecondary
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: theme.borderColor
            }

            Text {
                width: parent.width
                text: "⚠️ This action CANNOT be undone. All operational data will be permanently lost."
                font.pixelSize: 14
                font.weight: Font.Bold
                color: theme.dangerRed
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            // ✅ ENHANCED: Progress indicator with detailed status
            Column {
                width: parent.width
                spacing: theme.spacingSmall
                visible: globalDatabaseInitializer && globalDatabaseInitializer.isRunning

                Text {
                    width: parent.width
                    text: globalDatabaseInitializer ? globalDatabaseInitializer.currentOperation : ""
                    font.pixelSize: 12
                    color: theme.accentBlue
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Rectangle {
                    width: parent.width
                    height: 24
                    color: theme.darkBackground
                    border.color: theme.borderColor
                    border.width: 1
                    radius: 4

                    Rectangle {
                        width: parent.width > 4 ? (parent.width - 4) * ((globalDatabaseInitializer ? globalDatabaseInitializer.progress : 0) / 100) : 0
                        height: parent.height - 4
                        anchors.left: parent.left
                        anchors.leftMargin: 2
                        anchors.verticalCenter: parent.verticalCenter
                        color: theme.accentBlue
                        radius: 2

                        Behavior on width {
                            NumberAnimation { duration: 200 }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: (globalDatabaseInitializer ? globalDatabaseInitializer.progress : 0) + "%"
                        font.pixelSize: 11
                        font.weight: Font.Bold
                        color: theme.textPrimary
                    }
                }

                Text {
                    width: parent.width
                    text: "Please wait while the database is being rebuilt..."
                    font.pixelSize: 10
                    color: theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    font.italic: true
                }
            }

            Item { height: theme.spacingMedium }

            // Action buttons
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: theme.spacingMedium

                Button {
                    text: "Cancel"
                    enabled: !globalDatabaseInitializer.isRunning
                    onClicked: databaseResetDialog.close()

                    background: Rectangle {
                        color: parent.pressed ? "#4a5568" : theme.controlBackground
                        border.color: theme.borderColor
                        border.width: 1
                        radius: 4
                    }

                    contentItem: Text {
                        text: parent.text
                        color: theme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: globalDatabaseInitializer && globalDatabaseInitializer.isRunning ? "Resetting Database..." : "⚠️ RESET DATABASE"
                    enabled: globalDatabaseInitializer && !globalDatabaseInitializer.isRunning

                    onClicked: {
                        console.log("Database reset initiated by user")
                        if (globalDatabaseInitializer) {
                            globalDatabaseInitializer.resetDatabaseAsync()
                        }
                    }

                    background: Rectangle {
                        color: parent.pressed ? "#c53030" : theme.dangerRed
                        border.color: "#e53e3e"
                        border.width: 1
                        radius: 4
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "#ffffff"
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // ✅ ENHANCED: Database Reset Result Dialog
    Dialog {
        id: resetResultDialog
        property bool success: false
        property string message: ""

        title: success ? "✅ Database Reset Successful" : "❌ Database Reset Failed"
        modal: true
        anchors.centerIn: parent
        width: Math.min(500, parent.width * 0.7)

        background: Rectangle {
            color: theme.controlBackground
            border.color: resetResultDialog.success ? theme.successGreen : theme.dangerRed
            border.width: 2
            radius: 8
        }

        Column {
            anchors.fill: parent
            anchors.margins: theme.spacingLarge
            spacing: theme.spacingMedium

            Text {
                width: parent.width
                text: resetResultDialog.success ?
                      "✅ Database has been successfully reset and populated with fresh data!" :
                      "❌ Database reset failed. Please check the connection and try again."
                font.pixelSize: 16
                font.weight: Font.Bold
                color: resetResultDialog.success ? theme.successGreen : theme.dangerRed
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                width: parent.width
                text: resetResultDialog.message
                font.pixelSize: 12
                color: theme.textPrimary
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            // ✅ NEW: Additional info for successful reset
            Column {
                width: parent.width
                spacing: 4
                visible: resetResultDialog.success

                Text {
                    width: parent.width
                    text: "The following have been restored to default state:"
                    font.pixelSize: 11
                    color: theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: "• All signals set to RED (safe state)"
                    font.pixelSize: 10
                    color: theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: "• Point machines in configured positions"
                    font.pixelSize: 10
                    color: theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: "• Track segments cleared and ready for operation"
                    font.pixelSize: 10
                    color: theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "OK"
                onClicked: resetResultDialog.close()

                background: Rectangle {
                    color: parent.pressed ? theme.accentBlue : theme.controlBackground
                    border.color: theme.borderColor
                    border.width: 1
                    radius: 4
                }

                contentItem: Text {
                    text: parent.text
                    color: theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // Connection Test Result Dialog (unchanged)
    Dialog {
        id: connectionTestDialog
        property bool success: false
        property string message: ""

        title: success ? "✅ Connection Test Successful" : "❌ Connection Test Failed"
        modal: true
        anchors.centerIn: parent
        width: Math.min(400, parent.width * 0.6)

        background: Rectangle {
            color: theme.controlBackground
            border.color: connectionTestDialog.success ? theme.successGreen : theme.dangerRed
            border.width: 1
            radius: 8
        }

        Column {
            anchors.fill: parent
            anchors.margins: theme.spacingLarge
            spacing: theme.spacingMedium

            Text {
                width: parent.width
                text: connectionTestDialog.message
                font.pixelSize: 12
                color: theme.textPrimary
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "OK"
                onClicked: connectionTestDialog.close()

                background: Rectangle {
                    color: parent.pressed ? theme.accentBlue : theme.controlBackground
                    border.color: theme.borderColor
                    border.width: 1
                    radius: 4
                }

                contentItem: Text {
                    text: parent.text
                    color: theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // ✅ ENHANCED: Header with better status information
    header: Rectangle {
        height: 60
        color: theme.controlBackground
        border.color: theme.borderColor
        border.width: 1

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: theme.spacingMedium
            spacing: theme.spacingLarge

            Text {
                text: "RailFlux Control Panel"
                font.pixelSize: 18
                font.weight: Font.Bold
                color: theme.textPrimary
                anchors.verticalCenter: parent.verticalCenter
            }

            // ✅ ENHANCED: Connection status with real-time indicator
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: globalDatabaseManager && globalDatabaseManager.isConnected ? theme.successGreen : theme.dangerRed
                    anchors.verticalCenter: parent.verticalCenter

                    // Subtle pulsing animation for connected state
                    SequentialAnimation on opacity {
                        running: globalDatabaseManager && globalDatabaseManager.isConnected
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.5; duration: 1000 }
                        NumberAnimation { to: 1.0; duration: 1000 }
                    }
                }

                Text {
                    id: connectionStatus
                    text: globalDatabaseManager && globalDatabaseManager.isConnected ? "Database Connected" : "Database Disconnected"
                    font.pixelSize: 14
                    color: globalDatabaseManager && globalDatabaseManager.isConnected ? theme.successGreen : theme.dangerRed
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                text: Qt.formatDateTime(new Date(), "hh:mm:ss")
                font.pixelSize: 14
                color: theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter

                Timer {
                    interval: 1000
                    running: true
                    repeat: true
                    onTriggered: parent.text = Qt.formatDateTime(new Date(), "hh:mm:ss")
                }
            }
        }

        // ✅ NEW: Right side header controls
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.rightMargin: theme.spacingMedium
            spacing: theme.spacingSmall

            // Quick refresh button
            Button {
                width: 32
                height: 32
                text: "🔄"

                ToolTip.text: "Refresh all data from database"
                ToolTip.visible: hovered
                ToolTip.delay: 1000

                onClicked: {
                    console.log("Manual data refresh requested")
                    if (globalDatabaseManager && globalDatabaseManager.isConnected) {
                        stationLayout.refreshAllData()
                    }
                }

                background: Rectangle {
                    color: parent.hovered ? theme.accentBlue : "transparent"
                    border.color: theme.borderColor
                    border.width: 1
                    radius: 4
                }

                contentItem: Text {
                    text: parent.text
                    color: theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // Database management button
            Button {
                width: 80
                height: 32
                text: "Database"

                onClicked: databaseResetDialog.open()

                background: Rectangle {
                    color: parent.hovered ? theme.dangerRed : "transparent"
                    border.color: theme.dangerRed
                    border.width: 1
                    radius: 4
                }

                contentItem: Text {
                    text: parent.text
                    color: theme.dangerRed
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    Layouts.StationLayout {
        id: stationLayout
        anchors.fill: parent
        anchors.margins: theme.spacingMedium

        // Pass database manager to station layout
        dbManager: globalDatabaseManager
        onDatabaseResetRequested: {
            console.log("Opening database reset dialog from status panel")
            databaseResetDialog.open()
        }
    }
}
