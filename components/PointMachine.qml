// components/PointMachine.qml
import QtQuick
import "../data/StationData.js" as StationData

Item {
    id: pointMachine

    // ============================================================================
    // COMPONENT PROPERTIES
    // ============================================================================
    property string machineId: ""
    property string position: "NORMAL"              // "NORMAL" or "REVERSE"
    property string operatingStatus: "CONNECTED"    // "CONNECTED" or "IN_TRANSITION"
    property var junctionPoint: ({ row: 0, col: 0 })
    property var rootTrack: ({})
    property var normalTrack: ({})
    property var reverseTrack: ({})
    property int cellSize: 20
    property int transitionTime: 100

    // ============================================================================
    // VISUAL CONFIGURATION CONSTANTS
    // ============================================================================

    // **CONTAINER SIZING**
    readonly property real containerSizeMultiplier: 10.0        // cellSize * 10

    // **TRACK CONNECTION VISUAL PROPERTIES**
    readonly property real trackThickness: 8                    // Height of track rectangles
    readonly property real trackRadius: 2                       // Corner radius for tracks
    readonly property real railLineThickness: 1                 // Height of rail detail lines
    readonly property real railLineMargin: 1                    // Margin from track edges

    // **JUNCTION CONNECTION PROPERTIES**
    readonly property real junctionOverlapLength: 3            // How far to extend for overlap
    readonly property real junctionCapRadius: trackThickness * 1  // Junction cap size

    // **TRACK CONNECTION COLORS**
    readonly property color rootConnectionColor: "#00aa00"      // Green for root track
    readonly property color railLineColor: "#a6a6a6"           // Gray for rail details
    readonly property color junctionCapColor: "#2d3748"        // Dark gray for junction cap

    // **ACTIVE TRACK STATUS COLORS**
    readonly property color normalPositionColor: "#00ff00"     // Bright green for normal
    readonly property color reversePositionColor: "#ffaa00"    // Orange for reverse
    readonly property color transitionColor: "#ff6600"         // Orange-red for moving
    readonly property color errorColor: "#aa0000"              // Dark red for errors

    // **MOTOR INDICATOR SIZING**
    readonly property real motorSizeMultiplier: 0.6            // Motor size relative to cellSize
    readonly property real motorInnerSizeMultiplier: 0.4       // Inner indicator size relative to motor
    readonly property real motorBorderWidth: 2                 // Motor border thickness
    readonly property real motorRingBorderNormal: 1            // Normal status ring thickness
    readonly property real motorRingBorderActive: 3            // Transition status ring thickness

    // **MOTOR INDICATOR COLORS**
    readonly property color motorColorNormal: "#2d3748"        // Dark gray for normal operation
    readonly property color motorColorTransition: "#ff6600"    // Orange for transition
    readonly property color motorColorError: "#aa0000"         // Red for error
    readonly property color motorBorderColor: "#ffffff"        // White borders
    readonly property color motorInnerColor: "#ffffff"         // White inner indicator

    // **MOTOR POSITION ANGLES**
    readonly property real normalPositionAngle: 0              // 0 degrees for normal
    readonly property real reversePositionAngle: 45            // 45 degrees for reverse

    // **ANIMATION TIMING**
    readonly property int quickAnimationDuration: 100          // Fast animations (hover, etc.)
    readonly property int normalAnimationDuration: 300         // Standard animations
    readonly property int colorAnimationDuration: 150          // Color transition animations

    // **INTERACTION VISUAL PROPERTIES**
    readonly property real hoverOpacity: 0.1                   // Hover effect opacity
    readonly property real hoverRadius: 6                      // Hover effect corner radius

    // **TEXT AND LABELING**
    readonly property real labelFontSize: 8                    // Main label font size
    readonly property real statusFontSize: 7                   // Status text font size
    readonly property real textMargin: 4                       // Margin around text elements
    readonly property color textColor: "#ffffff"               // White text color
    readonly property string textFontFamily: "Arial"           // Professional font family

    // ============================================================================
    // POSITIONING AND SIZING
    // ============================================================================

    // Position at junction point
    x: junctionPoint.col * cellSize - width / 2
    y: junctionPoint.row * cellSize - height / 2
    width: cellSize * containerSizeMultiplier
    height: cellSize * containerSizeMultiplier

    signal pointMachineClicked(string machineId, string currentPosition)

    // ============================================================================
    // HELPER FUNCTIONS
    // ============================================================================

    function getRootTrackData() {
        return StationData.getTrackById(rootTrack.trackId);
    }

    function getRootEndpoint() {
        var trackData = getRootTrackData();
        if (!trackData) return { row: 0, col: 0 };
        return StationData.getTrackEndpoint(trackData, rootTrack.connectionEnd);
    }

    function getJunctionPixel() {
        return {
            x: junctionPoint.col * cellSize,
            y: junctionPoint.row * cellSize
        };
    }

    function getRootPixel() {
        var endpoint = getRootEndpoint();
        var offsetRow = rootTrack.offset ? (rootTrack.offset.row || 0) : 0;
        var offsetCol = rootTrack.offset ? (rootTrack.offset.col || 0) : 0;
        return {
            x: (endpoint.col + offsetCol) * cellSize,
            y: (endpoint.row + offsetRow) * cellSize
        };
    }

    function getActiveTrackInfo() {
        return (position === "NORMAL") ? normalTrack : reverseTrack;
    }

    function getActiveTrackData() {
        var activeInfo = getActiveTrackInfo();
        return StationData.getTrackById(activeInfo.trackId);
    }

    function getActiveEndpoint() {
        var trackData = getActiveTrackData();
        var activeInfo = getActiveTrackInfo();
        if (!trackData) return { row: 0, col: 0 };
        return StationData.getTrackEndpoint(trackData, activeInfo.connectionEnd);
    }

    function getActivePixel() {
        var endpoint = getActiveEndpoint();
        var activeInfo = getActiveTrackInfo();
        var offsetRow = activeInfo.offset ? (activeInfo.offset.row || 0) : 0;
        var offsetCol = activeInfo.offset ? (activeInfo.offset.col || 0) : 0;
        return {
            x: (endpoint.col + offsetCol) * cellSize,
            y: (endpoint.row + offsetRow) * cellSize
        };
    }

    function getActiveColor() {
        switch(operatingStatus) {
            case "CONNECTED":
                return position === "NORMAL" ? normalPositionColor : reversePositionColor;
            case "IN_TRANSITION":
                return transitionColor;
            default:
                return errorColor;
        }
    }

    function getMotorColor() {
        switch(operatingStatus) {
            case "CONNECTED": return motorColorNormal;
            case "IN_TRANSITION": return motorColorTransition;
            default: return motorColorError;
        }
    }

    // ============================================================================
    // VISUAL COMPONENTS
    // ============================================================================

    // **ROOT TRACK CONNECTION** - Extended for overlap
    Rectangle {
        id: rootConnection

        x: {
            var rootPx = getRootPixel();
            var junctionPx = getJunctionPixel();

            // Calculate direction vector for extension
            var deltaX = junctionPx.x - rootPx.x;
            var deltaY = junctionPx.y - rootPx.y;
            var length = Math.sqrt(deltaX * deltaX + deltaY * deltaY);

            // Extend beyond junction for overlap
            var extensionLength = junctionOverlapLength;
            var extendedX = rootPx.x - (deltaX / length) * extensionLength;

            return extendedX - pointMachine.x;
        }
        y: {
            var rootPx = getRootPixel();
            var junctionPx = getJunctionPixel();

            // Calculate direction vector for extension
            var deltaX = junctionPx.x - rootPx.x;
            var deltaY = junctionPx.y - rootPx.y;
            var length = Math.sqrt(deltaX * deltaX + deltaY * deltaY);

            // Extend beyond junction for overlap
            var extensionLength = junctionOverlapLength;
            var extendedY = rootPx.y - (deltaY / length) * extensionLength;

            return extendedY - pointMachine.y;
        }
        width: {
            var junctionPx = getJunctionPixel();
            var rootPx = getRootPixel();
            var baseLength = Math.sqrt(Math.pow(junctionPx.x - rootPx.x, 2) + Math.pow(junctionPx.y - rootPx.y, 2));

            // Add extension for overlap
            return baseLength + junctionOverlapLength;
        }
        height: trackThickness

        transformOrigin: Item.Left
        rotation: {
            var junctionPx = getJunctionPixel();
            var rootPx = getRootPixel();
            return Math.atan2(junctionPx.y - rootPx.y, junctionPx.x - rootPx.x) * 180 / Math.PI;
        }

        color: rootConnectionColor
        radius: trackRadius

        // **RAIL LINES**
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
    }

    // **ACTIVE TRACK CONNECTION** - Extended for overlap
    Rectangle {
        id: activeConnection

        x: {
            var junctionPx = getJunctionPixel();
            var activePx = getActivePixel();

            // Calculate direction vector for extension
            var deltaX = activePx.x - junctionPx.x;
            var deltaY = activePx.y - junctionPx.y;
            var length = Math.sqrt(deltaX * deltaX + deltaY * deltaY);

            // Extend backward beyond junction for overlap
            var extensionLength = junctionOverlapLength;
            var extendedX = junctionPx.x - (deltaX / length) * extensionLength;

            return extendedX - pointMachine.x;
        }
        y: {
            var junctionPx = getJunctionPixel();
            var activePx = getActivePixel();

            // Calculate direction vector for extension
            var deltaX = activePx.x - junctionPx.x;
            var deltaY = activePx.y - junctionPx.y;
            var length = Math.sqrt(deltaX * deltaX + deltaY * deltaY);

            // Extend backward beyond junction for overlap
            var extensionLength = junctionOverlapLength;
            var extendedY = junctionPx.y - (deltaY / length) * extensionLength;

            return extendedY - pointMachine.y;
        }
        width: {
            var junctionPx = getJunctionPixel();
            var activePx = getActivePixel();
            var baseLength = Math.sqrt(Math.pow(activePx.x - junctionPx.x, 2) + Math.pow(activePx.y - junctionPx.y, 2));

            // Add extension for overlap
            return baseLength + junctionOverlapLength;
        }
        height: trackThickness

        transformOrigin: Item.Left
        rotation: {
            var junctionPx = getJunctionPixel();
            var activePx = getActivePixel();
            return Math.atan2(activePx.y - junctionPx.y, activePx.x - junctionPx.x) * 180 / Math.PI;
        }

        color: getActiveColor()
        radius: trackRadius

        // **RAIL LINES**
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

        // **SMOOTH TRANSITION ANIMATIONS**
        Behavior on rotation {
            NumberAnimation {
                duration: operatingStatus === "IN_TRANSITION" ? transitionTime : normalAnimationDuration
                easing.type: Easing.OutQuart
            }
        }

        Behavior on width {
            NumberAnimation {
                duration: operatingStatus === "IN_TRANSITION" ? transitionTime : normalAnimationDuration
                easing.type: Easing.OutQuart
            }
        }

        Behavior on color {
            ColorAnimation {
                duration: colorAnimationDuration
            }
        }
    }

    // **JUNCTION CAP** - Seamless connection piece to eliminate gaps
    Rectangle {
        id: junctionCap

        width: junctionCapRadius * 2
        height: junctionCapRadius * 2
        radius: junctionCapRadius

        // Position at exact junction point
        x: {
            var junctionPx = getJunctionPixel();
            return junctionPx.x - pointMachine.x - junctionCapRadius;
        }
        y: {
            var junctionPx = getJunctionPixel();
            return junctionPx.y - pointMachine.y - junctionCapRadius;
        }

        // Use junction-specific color
        color: junctionCapColor

        // Ensure it's above the track rectangles
        z: 1

        // **JUNCTION CENTER INDICATOR**
        Rectangle {
            width: parent.width * 0.5
            height: parent.height * 0.5
            radius: width / 2
            anchors.centerIn: parent
            color: getActiveColor()
            opacity: 0.7

            Behavior on color {
                ColorAnimation {
                    duration: colorAnimationDuration
                }
            }
        }

        // **SUBTLE JUNCTION BORDER**
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: railLineColor
            border.width: 1
            radius: parent.radius
            opacity: 0.6
        }
    }

    // **MOTOR INDICATOR** - Enhanced with rail-style design
    Rectangle {
        id: motorIndicator
        width: cellSize * motorSizeMultiplier
        height: cellSize * motorSizeMultiplier
        radius: width / 2
        color: getMotorColor()
        border.color: motorBorderColor
        border.width: motorBorderWidth
        anchors.centerIn: parent

        // Ensure motor is above junction cap
        z: 2

        // **MOTOR POSITION INDICATOR**
        Rectangle {
            width: parent.width * motorInnerSizeMultiplier
            height: parent.height * motorInnerSizeMultiplier
            radius: width / 2
            color: motorInnerColor
            anchors.centerIn: parent
            rotation: position === "NORMAL" ? normalPositionAngle : reversePositionAngle

            Behavior on rotation {
                NumberAnimation {
                    duration: operatingStatus === "IN_TRANSITION" ? normalAnimationDuration : quickAnimationDuration
                    easing.type: Easing.OutCubic
                }
            }
        }

        // **STATUS INDICATOR RING**
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: operatingStatus === "IN_TRANSITION" ? transitionColor : motorBorderColor
            border.width: operatingStatus === "IN_TRANSITION" ? motorRingBorderActive : motorRingBorderNormal
            radius: width / 2

            Behavior on border.width {
                NumberAnimation { duration: colorAnimationDuration }
            }

            Behavior on border.color {
                ColorAnimation { duration: colorAnimationDuration }
            }
        }
    }

    // **CLICKABLE AREA**
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            console.log("Point machine clicked:", machineId, "Current position:", position, "Status:", operatingStatus)
            pointMachine.pointMachineClicked(machineId, position)
        }

        // **HOVER EFFECT**
        Rectangle {
            anchors.fill: parent
            color: "white"
            opacity: parent.containsMouse ? hoverOpacity : 0
            radius: hoverRadius

            Behavior on opacity {
                NumberAnimation { duration: quickAnimationDuration }
            }
        }
    }
}
