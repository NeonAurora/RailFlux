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

    // Professional railway color scheme
    color: "#1a1a1a"

    // **FIXED**: No alias needed - use globalDatabaseManager directly
    // property alias databaseManager: globalDatabaseManager  // ❌ Can't alias context properties

    // **CONNECT TO THE WORKING DATABASE MANAGER**
    Connections {
        target: globalDatabaseManager  // Context property from main.cpp

        function onConnectionStateChanged(isConnected) {
            console.log("Database connection changed:", isConnected)
            connectionStatus.text = isConnected ? "Connected" : "Disconnected"
            connectionStatus.color = isConnected ? theme.successGreen : theme.dangerRed
        }

        function onDataUpdated() {
            // Trigger UI updates when database data changes
            stationLayout.updateDisplay()
        }
    }

    // Theme object matching your best practices
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

            Text {
                id: connectionStatus
                // **FIXED**: Use globalDatabaseManager directly (no alias)
                text: globalDatabaseManager && globalDatabaseManager.isConnected ? "Connected" : "Disconnected"
                font.pixelSize: 14
                color: globalDatabaseManager && globalDatabaseManager.isConnected ? theme.successGreen : theme.dangerRed
                anchors.verticalCenter: parent.verticalCenter
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
    }

    Layouts.StationLayout {
        id: stationLayout
        anchors.fill: parent
        anchors.margins: theme.spacingMedium

        // **FIXED**: Pass globalDatabaseManager directly (no alias)
        dbManager: globalDatabaseManager
    }
}
