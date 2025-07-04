import QtQuick

Item {
    id: trackSegment

    property string segmentId: ""
    property real startRow: 0
    property real startCol: 0
    property real endRow: 0
    property real endCol: 0
    property bool isOccupied: false
    property int cellSize: 20
    property string trackType: "straight"

    // Visual properties matching your pygame colors
    property color trackColorNormal: "#eae0c8"  // Your TRACK_COLOR (234, 224, 200)
    property color trackColorOccupied: "#ff3232"  // Your train color when occupied
    property color railColor: "#ffffff"

    // Calculate actual pixel positions
    property real startX: startCol * cellSize
    property real startY: startRow * cellSize
    property real endX: endCol * cellSize
    property real endY: endRow * cellSize

    // Position and size calculations (your pygame logic)
    x: startX
    y: startY
    width: Math.sqrt(Math.pow(endX - startX, 2) + Math.pow(endY - startY, 2))
    height: 8
    rotation: Math.atan2(endY - startY, endX - startX) * 180 / Math.PI

    // **CLICKABLE FUNCTIONALITY**
    signal trackClicked(string segmentId, bool currentState)
    signal trackHovered(string segmentId)

    Rectangle {
        id: trackBed
        anchors.fill: parent
        color: isOccupied ? trackColorOccupied : trackColorNormal
        radius: 2

        // Your gap system - reduced width for gaps
        width: parent.width - 4  // 4px gap like your gap_size
        anchors.centerIn: parent

        // Rail lines (your white rail overlay)
        Rectangle {
            width: parent.width
            height: 1
            anchors.top: parent.top
            anchors.topMargin: 1
            color: railColor
        }

        Rectangle {
            width: parent.width
            height: 1
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            color: railColor
        }

        // **HOVER EFFECT** for professional UI
        Rectangle {
            anchors.fill: parent
            color: "white"
            opacity: hoverArea.containsMouse ? 0.2 : 0
            radius: parent.radius

            Behavior on opacity {
                NumberAnimation { duration: 150 }
            }
        }
    }

    // **CLICKABLE MOUSE AREA**
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            console.log("Track segment clicked:", segmentId, "Current occupied:", isOccupied)
            trackSegment.trackClicked(segmentId, isOccupied)
        }

        onEntered: {
            trackSegment.trackHovered(segmentId)
        }
    }

    // **DEBUG LABEL** (like your pygame debug output)
    Text {
        anchors.centerIn: parent
        text: segmentId
        color: "yellow"
        font.pixelSize: 8
        visible: false  // Set to true for debugging coordinates
    }
}
