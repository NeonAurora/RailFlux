// components/TrackSegment.qml
import QtQuick

Item {
    id: trackSegment

    // ============================================================================
    // COMPONENT PROPERTIES
    // ============================================================================
    property string segmentId: ""
    property string segmentName: ""           // ✅ NEW: Track segment name from database
    property real startRow: 0
    property real startCol: 0
    property real endRow: 0
    property real endCol: 0
    property bool isOccupied: false
    property bool isAssigned: false
    property string occupiedBy: ""            // ✅ NEW: What/who occupies this track
    property bool isActive: true              // ✅ NEW: Whether track is active/in-service
    property int cellSize: 20
    property string trackType: "STRAIGHT"     // ✅ ENHANCED: Track type from database

    // ============================================================================
    // VISUAL CONFIGURATION CONSTANTS
    // ============================================================================

    // **TRACK VISUAL PROPERTIES**
    readonly property real trackThickness: 8
    readonly property real trackRadius: 2
    readonly property real containerPadding: 8
    readonly property real minimumContainerSize: 20

    // **RAIL LINE PROPERTIES**
    readonly property real railLineThickness: 1
    readonly property real railLineMargin: 1

    // **✅ ENHANCED: TRACK STATE COLORS WITH TRACK TYPE SUPPORT**
    readonly property color trackColorNormal: getTrackTypeColor()
    readonly property color trackColorOccupied: "#ff3232"       // Red for occupied
    readonly property color trackColorAssigned: "#ffff00"      // Yellow for assigned
    readonly property color trackColorInactive: "#606060"      // Dark gray for inactive
    readonly property color railLineColor: "#a6a6a6"

    // **✅ NEW: TRACK TYPE SPECIFIC COLORS**
    readonly property color straightTrackColor: "#a6a6a6"      // Standard gray
    readonly property color curvedTrackColor: "#9999aa"        // Slightly blue-gray
    readonly property color sidingTrackColor: "#aa9966"        // Brown-ish for sidings
    readonly property color platformTrackColor: "#66aa99"      // Teal for platform tracks
    readonly property color yardTrackColor: "#996699"          // Purple-ish for yard tracks

    // **INTERACTION VISUAL PROPERTIES**
    readonly property real hoverOpacity: 0.2
    readonly property color hoverColor: "white"
    readonly property int hoverAnimationDuration: 150

    // **✅ NEW: OCCUPIED BY LABEL PROPERTIES**
    readonly property real occupiedLabelFontSize: 7
    readonly property color occupiedLabelColor: "#ffffff"
    readonly property color occupiedLabelBackground: "#000000"
    readonly property real occupiedLabelOpacity: 0.8

    // **DEBUG VISUAL PROPERTIES**
    readonly property real debugTextPadding: 6
    readonly property real debugBackgroundPadding: 4
    readonly property color debugBackgroundColor: "#000000"
    readonly property color debugTextColor: "yellow"
    readonly property real debugTextOpacity: 0.8
    readonly property real debugTextSize: 7
    readonly property real debugBorderRadius: 2

    // **CONTAINER DEBUG PROPERTIES**
    readonly property real debugBorderWidth: 1
    readonly property real debugBorderOpacity: 0.4
    readonly property color debugBorderColorDiagonal: "red"
    readonly property color debugBorderColorStraight: "cyan"

    // **CURSOR AND INTERACTION**
    readonly property int clickCursor: Qt.PointingHandCursor

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

    // **✅ ENHANCED: TRACK STATE COLOR LOGIC WITH INACTIVE SUPPORT**
    readonly property color currentTrackColor: {
        if (!isActive) return trackColorInactive;           // Highest priority: Inactive tracks
        if (isAssigned) return trackColorAssigned;          // High priority: Assignment
        if (isOccupied) return trackColorOccupied;         // Medium priority: Occupation
        return trackColorNormal;                           // Default: Normal state (track type specific)
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
    // ✅ NEW: HELPER FUNCTIONS FOR TRACK TYPE COLORS
    // ============================================================================
    function getTrackTypeColor() {
        switch(trackType.toUpperCase()) {
            case "STRAIGHT": return straightTrackColor
            case "CURVED": return curvedTrackColor
            case "SIDING": return sidingTrackColor
            case "PLATFORM": return platformTrackColor
            case "YARD": return yardTrackColor
            default: return straightTrackColor  // Safe default
        }
    }

    function getTrackTypeDisplayName() {
        switch(trackType.toUpperCase()) {
            case "STRAIGHT": return "Main Line"
            case "CURVED": return "Curved Track"
            case "SIDING": return "Siding"
            case "PLATFORM": return "Platform"
            case "YARD": return "Yard Track"
            default: return trackType
        }
    }

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

        // **✅ ENHANCED: INACTIVE TRACK VISUAL EFFECT**
        opacity: isActive ? 1.0 : 0.6

        // **✅ NEW: DASHED PATTERN FOR INACTIVE TRACKS**
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: isActive ? "transparent" : "#ffffff"
            border.width: isActive ? 0 : 1
            radius: parent.radius

            // Dashed border effect for inactive tracks
            visible: !isActive
            opacity: 0.5
        }

        // **RAIL LINES** - Visual track details
        Rectangle {
            width: parent.width
            height: railLineThickness
            anchors.top: parent.top
            anchors.topMargin: railLineMargin
            color: railLineColor
            opacity: isActive ? 1.0 : 0.5  // ✅ NEW: Dimmed when inactive
        }

        Rectangle {
            width: parent.width
            height: railLineThickness
            anchors.bottom: parent.bottom
            anchors.bottomMargin: railLineMargin
            color: railLineColor
            opacity: isActive ? 1.0 : 0.5  // ✅ NEW: Dimmed when inactive
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

    // **✅ NEW: OCCUPIED BY LABEL** - Shows what occupies the track
    Rectangle {
        id: occupiedByLabel
        anchors.centerIn: trackBed
        width: occupiedByText.contentWidth + 8
        height: occupiedByText.contentHeight + 4
        color: occupiedLabelBackground
        opacity: occupiedLabelOpacity
        radius: 2
        visible: isOccupied && occupiedBy !== ""

        Text {
            id: occupiedByText
            anchors.centerIn: parent
            text: occupiedBy
            color: occupiedLabelColor
            font.pixelSize: occupiedLabelFontSize
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // **✅ NEW: TRACK NAME LABEL** - Shows segment name for important tracks
    Text {
        id: trackNameLabel
        anchors.top: trackBed.bottom
        anchors.horizontalCenter: trackBed.horizontalCenter
        anchors.topMargin: 2
        text: segmentName
        color: "#cccccc"
        font.pixelSize: 6
        font.family: "Arial"
        visible: segmentName !== "" && trackType === "PLATFORM"  // Only show for platform tracks
        horizontalAlignment: Text.AlignHCenter
    }

    // **MOUSE INTERACTION AREA**
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: isActive ? clickCursor : Qt.ForbiddenCursor  // ✅ NEW: Different cursor for inactive

        onClicked: {
            if (!isActive) {
                console.log("Track segment inactive:", segmentId, "- Click ignored")
                return
            }

            console.log("Track segment clicked:", segmentId,
                       "Name:", segmentName || "Unnamed",
                       "Type:", getTrackTypeDisplayName(),
                       "Coordinates:", "(" + startRow + "," + startCol + ") to (" + endRow + "," + endCol + ")",
                       "State:", isAssigned ? "ASSIGNED" : (isOccupied ? "OCCUPIED by " + occupiedBy : "NORMAL"),
                       "Direction:", isHorizontal ? "H" : (isVertical ? "V" : (isTopLeftToBottomRight ? "TL→BR" : "BL→TR")),
                       "Active:", isActive)
            trackSegment.trackClicked(segmentId, isOccupied)
        }

        onEntered: {
            trackSegment.trackHovered(segmentId)
            // ✅ NEW: Enhanced hover information
            console.log("🚂 Track Hover:", segmentId,
                       "Type:", getTrackTypeDisplayName(),
                       "Status:", isActive ? "Active" : "Inactive",
                       isOccupied ? ("Occupied by: " + occupiedBy) : "Free")
        }
    }

    // **✅ ENHANCED: DEBUG TEXT WITH NEW FIELDS**
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
            text: segmentId + (segmentName ? " (" + segmentName + ")" : "") +
                  "\n" + getTrackTypeDisplayName() +
                  "\n(" + startRow + "," + startCol + ")→(" + endRow + "," + endCol + ")" +
                  "\n" + (isActive ? "ACTIVE" : "INACTIVE") +
                  "\n" + (isAssigned ? "ASSIGNED" : (isOccupied ? "OCCUPIED" + (occupiedBy ? " by " + occupiedBy : "") : "NORMAL"))
            color: debugTextColor
            font.pixelSize: debugTextSize
            visible: false  // ✅ Set to true to verify all data fields
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

    // **✅ NEW: INACTIVE TRACK OVERLAY** - Visual indication for out-of-service tracks
    Rectangle {
        anchors.fill: trackBed
        color: "transparent"
        border.color: "#ff6600"
        border.width: 2
        radius: trackBed.radius
        visible: !isActive
        opacity: 0.7

        // "X" pattern for inactive tracks
        Rectangle {
            width: parent.width * 1.414  // √2 for diagonal
            height: 1
            color: "#ff6600"
            anchors.centerIn: parent
            rotation: 45
            opacity: 0.6
        }
        Rectangle {
            width: parent.width * 1.414
            height: 1
            color: "#ff6600"
            anchors.centerIn: parent
            rotation: -45
            opacity: 0.6
        }
    }
}
