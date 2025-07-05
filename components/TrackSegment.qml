// components/TrackSegment.qml
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

    // Visual properties
    property color trackColorNormal: "#a6a6a6"
    property color trackColorOccupied: "#ff3232"
    property color railColor: "#a6a6a6"

    // **DIRECTION DETECTION**
    property bool isHorizontal: Math.abs(endCol - startCol) > Math.abs(endRow - startRow)
    property bool isVertical: Math.abs(endRow - startRow) > Math.abs(endCol - startCol)
    property bool isDiagonal: !isHorizontal && !isVertical

    // **DIAGONAL DIRECTION DETECTION**
    property bool isTopLeftToBottomRight: isDiagonal && (endRow > startRow)
    property bool isBottomLeftToTopRight: isDiagonal && (endRow < startRow)

    // Convert to pixel positions - NO PADDING
    property real startX: startCol * cellSize
    property real startY: startRow * cellSize
    property real endX: endCol * cellSize
    property real endY: endRow * cellSize

    // **CALCULATE CONTAINER DIMENSIONS**
    property real containerWidth: Math.max(Math.abs(endX - startX) + 16, 20)
    property real containerHeight: Math.max(Math.abs(endY - startY) + 16, 20)

    // **POSITIONING** - Position container to encompass the track
    x: Math.min(startX, endX) - 8  // 8px padding
    y: Math.min(startY, endY) - 8  // 8px padding
    width: containerWidth
    height: containerHeight

    // **CLICKABLE FUNCTIONALITY**
    signal trackClicked(string segmentId, bool currentState)
    signal trackHovered(string segmentId)

    Rectangle {
        id: trackBed

        // **POSITION TRACK WITHIN CONTAINER**
        x: startX - parent.x
        y: startY - parent.y
        width: Math.sqrt(Math.pow(endX - startX, 2) + Math.pow(endY - startY, 2))
        height: 8

        transformOrigin: Item.Left
        rotation: Math.atan2(endY - startY, endX - startX) * 180 / Math.PI

        color: isOccupied ? trackColorOccupied : trackColorNormal
        radius: 2

        // Rail lines
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

        // **HOVER EFFECT**
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

    // **MOUSE AREA** - Covers entire container for reliable click detection
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            console.log("Track segment clicked:", segmentId,
                       "Coordinates:", "(" + startRow + "," + startCol + ") to (" + endRow + "," + endCol + ")",
                       "Direction:", isHorizontal ? "H" : (isVertical ? "V" : (isTopLeftToBottomRight ? "TL→BR" : "BL→TR")))
            trackSegment.trackClicked(segmentId, isOccupied)
        }

        onEntered: trackSegment.trackHovered(segmentId)
    }

    // **DEBUG TEXT WITH BACKGROUND**
    Rectangle {
        id: debugBackground
        anchors.centerIn: parent
        width: debugText.contentWidth + 6
        height: debugText.contentHeight + 4
        color: "#000000"
        opacity: 0.8
        radius: 2
        visible: debugText.visible

        Text {
            id: debugText
            anchors.centerIn: parent
            text: segmentId + "\n(" + startRow + "," + startCol + ")\n→(" + endRow + "," + endCol + ")"
            color: "yellow"
            font.pixelSize: 7
            visible: false  // Set to true to verify coordinates
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // **CONTAINER BOUNDS DEBUG** (shows the clickable area)
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: isDiagonal ? "red" : "cyan"
        border.width: 1
        opacity: 0.4
        visible: false  // Set to true to see clickable bounds
    }
}
