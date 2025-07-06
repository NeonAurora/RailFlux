// components/TrackSegment.qml
import QtQuick

Item {
    id: trackSegment

    // ============================================================================
    // COMPONENT PROPERTIES
    // ============================================================================
    property string segmentId: ""
    property real startRow: 0
    property real startCol: 0
    property real endRow: 0
    property real endCol: 0
    property bool isOccupied: false
    property bool isAssigned: false  // NEW: Assignment state
    property int cellSize: 20
    property string trackType: "straight"

    // ============================================================================
    // VISUAL CONFIGURATION CONSTANTS
    // ============================================================================

    // **TRACK VISUAL PROPERTIES**
    readonly property real trackThickness: 8                    // Height of track rectangles
    readonly property real trackRadius: 2                       // Corner radius for tracks
    readonly property real containerPadding: 8                  // Padding around track for click area
    readonly property real minimumContainerSize: 20             // Minimum container dimensions

    // **RAIL LINE PROPERTIES**
    readonly property real railLineThickness: 1                 // Height of rail detail lines
    readonly property real railLineMargin: 1                    // Margin from track edges

    // **TRACK STATE COLORS**
    readonly property color trackColorNormal: "#a6a6a6"         // Gray for normal track
    readonly property color trackColorOccupied: "#ff3232"       // Red for occupied track
    readonly property color trackColorAssigned: "#ffff00"      // Bright yellow for assigned track
    readonly property color railLineColor: "#a6a6a6"           // Gray for rail details

    // **INTERACTION VISUAL PROPERTIES**
    readonly property real hoverOpacity: 0.2                    // Hover effect opacity
    readonly property color hoverColor: "white"                 // Hover overlay color
    readonly property int hoverAnimationDuration: 150           // Hover animation timing

    // **DEBUG VISUAL PROPERTIES**
    readonly property real debugTextPadding: 6                  // Padding around debug text
    readonly property real debugBackgroundPadding: 4            // Background padding for debug text
    readonly property color debugBackgroundColor: "#000000"     // Debug text background
    readonly property color debugTextColor: "yellow"            // Debug text color
    readonly property real debugTextOpacity: 0.8               // Debug background opacity
    readonly property real debugTextSize: 7                     // Debug font size
    readonly property real debugBorderRadius: 2                 // Debug background radius

    // **CONTAINER DEBUG PROPERTIES**
    readonly property real debugBorderWidth: 1                  // Debug border thickness
    readonly property real debugBorderOpacity: 0.4             // Debug border opacity
    readonly property color debugBorderColorDiagonal: "red"     // Debug border for diagonal tracks
    readonly property color debugBorderColorStraight: "cyan"    // Debug border for straight tracks

    // **CURSOR AND INTERACTION**
    readonly property int clickCursor: Qt.PointingHandCursor    // Mouse cursor on hover

    // ============================================================================
    // COMPUTED PROPERTIES
    // ============================================================================

    // **DIRECTION DETECTION**
    readonly property bool isHorizontal: Math.abs(endCol - startCol) > Math.abs(endRow - startRow)
    readonly property bool isVertical: Math.abs(endRow - startRow) > Math.abs(endCol - startCol)
    readonly property bool isDiagonal: !isHorizontal && !isVertical

    // **DIAGONAL DIRECTION DETECTION**
    readonly property bool isTopLeftToBottomRight: isDiagonal && (endRow > startRow)
    readonly property bool isBottomLeftToTopRight: isDiagonal && (endRow < startRow)

    // **PIXEL POSITION CALCULATIONS**
    readonly property real startX: startCol * cellSize
    readonly property real startY: startRow * cellSize
    readonly property real endX: endCol * cellSize
    readonly property real endY: endRow * cellSize

    // **CONTAINER DIMENSIONS**
    readonly property real containerWidth: Math.max(Math.abs(endX - startX) + (containerPadding * 2), minimumContainerSize)
    readonly property real containerHeight: Math.max(Math.abs(endY - startY) + (containerPadding * 2), minimumContainerSize)

    // **TRACK STATE COLOR LOGIC**
    readonly property color currentTrackColor: {
        if (isAssigned) return trackColorAssigned;        // Highest priority: Assignment
        if (isOccupied) return trackColorOccupied;      // Medium priority: Occupation
        return trackColorNormal;                        // Default: Normal state
    }

    // ============================================================================
    // POSITIONING AND SIZING
    // ============================================================================

    // Position container to encompass the track with padding
    x: Math.min(startX, endX) - containerPadding
    y: Math.min(startY, endY) - containerPadding
    width: containerWidth
    height: containerHeight

    // ============================================================================
    // SIGNALS
    // ============================================================================
    signal trackClicked(string segmentId, bool currentState)
    signal trackHovered(string segmentId)

    // ============================================================================
    // VISUAL COMPONENTS
    // ============================================================================

    // **MAIN TRACK RECTANGLE**
    Rectangle {
        id: trackBed

        // **POSITION TRACK WITHIN CONTAINER**
        x: startX - parent.x
        y: startY - parent.y
        width: Math.sqrt(Math.pow(endX - startX, 2) + Math.pow(endY - startY, 2))
        height: trackThickness

        transformOrigin: Item.Left
        rotation: Math.atan2(endY - startY, endX - startX) * 180 / Math.PI

        color: currentTrackColor
        radius: trackRadius

        // **RAIL LINES** - Visual track details
        Rectangle {
            width: parent.width
            height: railLineThickness
            anchors.top: parent.top
            anchors.topMargin: railLineMargin
            color: railLineColor
        }

        Rectangle {
            width: parent.width
            height: railLineThickness
            anchors.bottom: parent.bottom
            anchors.bottomMargin: railLineMargin
            color: railLineColor
        }

        // **HOVER EFFECT**
        Rectangle {
            anchors.fill: parent
            color: hoverColor
            opacity: hoverArea.containsMouse ? hoverOpacity : 0
            radius: parent.radius

            Behavior on opacity {
                NumberAnimation { duration: hoverAnimationDuration }
            }
        }
    }

    // **MOUSE INTERACTION AREA**
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: clickCursor

        onClicked: {
            console.log("Track segment clicked:", segmentId,
                       "Coordinates:", "(" + startRow + "," + startCol + ") to (" + endRow + "," + endCol + ")",
                       "State:", isAssigned ? "ASSIGNED" : (isOccupied ? "OCCUPIED" : "NORMAL"),
                       "Direction:", isHorizontal ? "H" : (isVertical ? "V" : (isTopLeftToBottomRight ? "TL→BR" : "BL→TR")))
            trackSegment.trackClicked(segmentId, isOccupied)
        }

        onEntered: trackSegment.trackHovered(segmentId)
    }

    // **DEBUG TEXT WITH BACKGROUND** - Development aid
    Rectangle {
        id: debugBackground
        anchors.centerIn: parent
        width: debugText.contentWidth + debugTextPadding
        height: debugText.contentHeight + debugBackgroundPadding
        color: debugBackgroundColor
        opacity: debugTextOpacity
        radius: debugBorderRadius
        visible: debugText.visible

        Text {
            id: debugText
            anchors.centerIn: parent
            text: segmentId + "\n(" + startRow + "," + startCol + ")\n→(" + endRow + "," + endCol + ")" +
                  "\n" + (isAssigned ? "ASSIGNED" : (isOccupied ? "OCCUPIED" : "NORMAL"))
            color: debugTextColor
            font.pixelSize: debugTextSize
            visible: false  // Set to true to verify coordinates and state
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // **CONTAINER BOUNDS DEBUG** - Shows clickable area
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: isDiagonal ? debugBorderColorDiagonal : debugBorderColorStraight
        border.width: debugBorderWidth
        opacity: debugBorderOpacity
        visible: false  // Set to true to see clickable bounds
    }
}
