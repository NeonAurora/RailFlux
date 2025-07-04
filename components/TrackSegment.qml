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

    // **PADDING CONTROLS** - Grid-based
    property real rowPadding: 2
    property real colPadding: 2

    // Visual properties
    property color trackColorNormal: "#eae0c8"
    property color trackColorOccupied: "#ff3232"
    property color railColor: "#ffffff"

    // **DIRECTION DETECTION**
    property bool isHorizontal: Math.abs(endCol - startCol) > Math.abs(endRow - startRow)
    property bool isVertical: Math.abs(endRow - startRow) > Math.abs(endCol - startCol)
    property bool isDiagonal: !isHorizontal && !isVertical

    // **DIAGONAL DIRECTION DETECTION**
    property bool isTopLeftToBottomRight: isDiagonal && (endRow > startRow)
    property bool isBottomLeftToTopRight: isDiagonal && (endRow < startRow)

    // **CALCULATE PADDED COORDINATES**
    property real paddedStartRow: {
        if (isVertical) return startRow + rowPadding
        if (isHorizontal) return startRow
        if (isTopLeftToBottomRight) return startRow + rowPadding * 0.5
        if (isBottomLeftToTopRight) return startRow - rowPadding * 0.5
        return startRow
    }

    property real paddedStartCol: {
        if (isHorizontal) return startCol + colPadding
        if (isVertical) return startCol
        if (isTopLeftToBottomRight) return startCol + colPadding * 0.5
        if (isBottomLeftToTopRight) return startCol + colPadding * 0.5
        return startCol
    }

    property real paddedEndRow: {
        if (isVertical) return endRow - rowPadding
        if (isHorizontal) return endRow
        if (isTopLeftToBottomRight) return endRow - rowPadding * 0.5
        if (isBottomLeftToTopRight) return endRow + rowPadding * 0.5
        return endRow
    }

    property real paddedEndCol: {
        if (isHorizontal) return endCol - colPadding
        if (isVertical) return endCol
        if (isTopLeftToBottomRight) return endCol - colPadding * 0.5
        if (isBottomLeftToTopRight) return endCol - colPadding * 0.5
        return endCol
    }

    // Convert to pixel positions
    property real paddedStartX: paddedStartCol * cellSize
    property real paddedStartY: paddedStartRow * cellSize
    property real paddedEndX: paddedEndCol * cellSize
    property real paddedEndY: paddedEndRow * cellSize

    // **FIXED POSITIONING** - Position the container at start point
    x: paddedStartX
    y: paddedStartY
    width: Math.sqrt(Math.pow(paddedEndX - paddedStartX, 2) + Math.pow(paddedEndY - paddedStartY, 2))
    height: 8

    // **CLICKABLE FUNCTIONALITY**
    signal trackClicked(string segmentId, bool currentState)
    signal trackHovered(string segmentId)

    Rectangle {
        id: trackBed

        // **KEY FIX**: Position rectangle at origin (0,0) within the Item
        x: 0
        y: 0
        width: parent.width
        height: parent.height

        // **KEY FIX**: Set transform origin to left edge for proper rotation
        transformOrigin: Item.Left
        rotation: Math.atan2(paddedEndY - paddedStartY, paddedEndX - paddedStartX) * 180 / Math.PI

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

    // **CLICKABLE MOUSE AREA**
    MouseArea {
        id: hoverArea
        anchors.fill: trackBed  // Fill the rotated rectangle
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            console.log("Track segment clicked:", segmentId,
                       "Original:", "(" + startRow + "," + startCol + ") to (" + endRow + "," + endCol + ")",
                       "Padded:", "(" + paddedStartRow.toFixed(1) + "," + paddedStartCol.toFixed(1) + ") to (" + paddedEndRow.toFixed(1) + "," + paddedEndCol.toFixed(1) + ")",
                       "Direction:", isHorizontal ? "H" : (isVertical ? "V" : (isTopLeftToBottomRight ? "TL→BR" : "BL→TR")))
            trackSegment.trackClicked(segmentId, isOccupied)
        }

        onEntered: trackSegment.trackHovered(segmentId)
    }

    // **DEBUG COORDINATES** (temporary for verification)
    Text {
        anchors.centerIn: parent
        text: "(" + startRow + "," + startCol + ")\n→(" + endRow + "," + endCol + ")"
        color: "yellow"
        font.pixelSize: 12
        visible: true  // Set to true to verify coordinates
        horizontalAlignment: Text.AlignHCenter
    }
}
