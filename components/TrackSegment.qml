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
    property real rowPadding: 0
    property real colPadding: 0

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

    // **CALCULATE CONTAINER DIMENSIONS** (only set once, no errors)
    property real containerWidth: Math.max(Math.abs(paddedEndX - paddedStartX) + 16, 20)
    property real containerHeight: Math.max(Math.abs(paddedEndY - paddedStartY) + 16, 20)

    // **FIXED POSITIONING** - Position container to encompass the track
    x: Math.min(paddedStartX, paddedEndX) - 8  // 8px padding
    y: Math.min(paddedStartY, paddedEndY) - 8  // 8px padding
    width: containerWidth
    height: containerHeight

    // **CLICKABLE FUNCTIONALITY**
    signal trackClicked(string segmentId, bool currentState)
    signal trackHovered(string segmentId)

    Rectangle {
        id: trackBed

        // **POSITION TRACK WITHIN CONTAINER**
        x: paddedStartX - parent.x
        y: paddedStartY - parent.y
        width: Math.sqrt(Math.pow(paddedEndX - paddedStartX, 2) + Math.pow(paddedEndY - paddedStartY, 2))
        height: 8

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

    // **LARGER MOUSE AREA** - Covers entire container for reliable click detection
    MouseArea {
        id: hoverArea
        anchors.fill: parent
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
