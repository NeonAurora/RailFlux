// components/PointMachine.qml
import QtQuick

Item {
    id: pointMachine

    // ============================================================================
    // COMPONENT PROPERTIES
    // ============================================================================
    property string machineId: ""
    property string machineName: ""                     // ✅ NEW
    property string position: "NORMAL"                  // "NORMAL" or "REVERSE" (converted from 1/2)
    property string operatingStatus: "CONNECTED"       // "CONNECTED" or "IN_TRANSITION"
    property var junctionPoint: ({ row: 0, col: 0 })
    property var rootTrack: ({})
    property var normalTrack: ({})
    property var reverseTrack: ({})
    property int cellSize: 20
    property int transitionTime: 3000                   // ✅ NEW
    property bool isLocked: false                       // ✅ NEW
    property string lockReason: ""                      // ✅ NEW

    // ✅ NEW: Track data lookup function (passed from parent)
    property var trackDataLookup: null

    // ============================================================================
    // VISUAL CONFIGURATION CONSTANTS (unchanged)
    // ============================================================================
    readonly property real containerSizeMultiplier: 10.0
    readonly property real trackThickness: 8
    readonly property real trackRadius: 2
    readonly property real railLineThickness: 1
    readonly property real railLineMargin: 1
    readonly property real junctionOverlapLength: 3
    readonly property real junctionCapRadius: trackThickness * 1

    // **TRACK CONNECTION COLORS**
    readonly property color rootConnectionColor: "#00aa00"
    readonly property color railLineColor: "#a6a6a6"
    readonly property color junctionCapColor: "#2d3748"

    // **ACTIVE TRACK STATUS COLORS**
    readonly property color normalPositionColor: "#00ff00"
    readonly property color reversePositionColor: "#ffaa00"
    readonly property color transitionColor: "#ff6600"
    readonly property color errorColor: "#aa0000"
    readonly property color lockedColor: "#ff0000"      // ✅ NEW

    // **MOTOR INDICATOR SIZING**
    readonly property real motorSizeMultiplier: 0.6
    readonly property real motorInnerSizeMultiplier: 0.4
    readonly property real motorBorderWidth: 2
    readonly property real motorRingBorderNormal: 1
    readonly property real motorRingBorderActive: 3

    // **MOTOR INDICATOR COLORS**
    readonly property color motorColorNormal: "#2d3748"
    readonly property color motorColorTransition: "#ff6600"
    readonly property color motorColorError: "#aa0000"
    readonly property color motorColorLocked: "#ff0000" // ✅ NEW
    readonly property color motorBorderColor: "#ffffff"
    readonly property color motorInnerColor: "#ffffff"

    // **MOTOR POSITION ANGLES**
    readonly property real normalPositionAngle: 0
    readonly property real reversePositionAngle: 45

    // **ANIMATION TIMING**
    readonly property int quickAnimationDuration: 100
    readonly property int normalAnimationDuration: 300
    readonly property int colorAnimationDuration: 150

    // **INTERACTION VISUAL PROPERTIES**
    readonly property real hoverOpacity: 0.1
    readonly property real hoverRadius: 6

    // ============================================================================
    // POSITIONING AND SIZING (unchanged)
    // ============================================================================
    x: junctionPoint.col * cellSize - width / 2
    y: junctionPoint.row * cellSize - height / 2
    width: cellSize * containerSizeMultiplier
    height: cellSize * containerSizeMultiplier

    signal pointMachineClicked(string machineId, string currentPosition)

    // ============================================================================
    // ✅ UPDATED: HELPER FUNCTIONS (database-aware)
    // ============================================================================

    function getRootTrackData() {
        if (!trackDataLookup || !rootTrack.trackId) return null;
        return trackDataLookup(rootTrack.trackId);
    }

    function getRootEndpoint() {
        var trackData = getRootTrackData();
        if (!trackData) return { row: 0, col: 0 };

        if (rootTrack.connectionEnd === "START") {
            return { row: trackData.startRow, col: trackData.startCol };
        } else {
            return { row: trackData.endRow, col: trackData.endCol };
        }
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
        if (!trackDataLookup || !activeInfo.trackId) return null;
        return trackDataLookup(activeInfo.trackId);
    }

    function getActiveEndpoint() {
        var trackData = getActiveTrackData();
        var activeInfo = getActiveTrackInfo();
        if (!trackData) return { row: 0, col: 0 };

        if (activeInfo.connectionEnd === "START") {
            return { row: trackData.startRow, col: trackData.startCol };
        } else {
            return { row: trackData.endRow, col: trackData.endCol };
        }
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
        if (isLocked) return lockedColor;

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
        if (isLocked) return motorColorLocked;

        switch(operatingStatus) {
            case "CONNECTED": return motorColorNormal;
            case "IN_TRANSITION": return motorColorTransition;
            default: return motorColorError;
        }
    }

    // ============================================================================
    // VISUAL COMPONENTS (unchanged structure, updated colors)
    // ============================================================================

    // **ROOT TRACK CONNECTION**
    Rectangle {
        id: rootConnection

        x: {
            var rootPx = getRootPixel();
            var junctionPx = getJunctionPixel();
            var deltaX = junctionPx.x - rootPx.x;
            var deltaY = junctionPx.y - rootPx.y;
            var length = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
            var extensionLength = junctionOverlapLength;
            var extendedX = rootPx.x - (deltaX / length) * extensionLength;
            return extendedX - pointMachine.x;
        }
        y: {
            var rootPx = getRootPixel();
            var junctionPx = getJunctionPixel();
            var deltaX = junctionPx.x - rootPx.x;
            var deltaY = junctionPx.y - rootPx.y;
            var length = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
            var extensionLength = junctionOverlapLength;
            var extendedY = rootPx.y - (deltaY / length) * extensionLength;
            return extendedY - pointMachine.y;
        }
        width: {
            var junctionPx = getJunctionPixel();
            var rootPx = getRootPixel();
            var baseLength = Math.sqrt(Math.pow(junctionPx.x - rootPx.x, 2) + Math.pow(junctionPx.y - rootPx.y, 2));
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

    // **ACTIVE TRACK CONNECTION**
    Rectangle {
        id: activeConnection

        x: {
            var junctionPx = getJunctionPixel();
            var activePx = getActivePixel();
            var deltaX = activePx.x - junctionPx.x;
            var deltaY = activePx.y - junctionPx.y;
            var length = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
            var extensionLength = junctionOverlapLength;
            var extendedX = junctionPx.x - (deltaX / length) * extensionLength;
            return extendedX - pointMachine.x;
        }
        y: {
            var junctionPx = getJunctionPixel();
            var activePx = getActivePixel();
            var deltaX = activePx.x - junctionPx.x;
            var deltaY = activePx.y - junctionPx.y;
            var length = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
            var extensionLength = junctionOverlapLength;
            var extendedY = junctionPx.y - (deltaY / length) * extensionLength;
            return extendedY - pointMachine.y;
        }
        width: {
            var junctionPx = getJunctionPixel();
            var activePx = getActivePixel();
            var baseLength = Math.sqrt(Math.pow(activePx.x - junctionPx.x, 2) + Math.pow(activePx.y - junctionPx.y, 2));
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

        Behavior on rotation {
            RotationAnimation {
                duration: operatingStatus === "IN_TRANSITION" ? transitionTime : normalAnimationDuration
                easing.type: Easing.OutQuart
                direction: RotationAnimation.Shortest
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

    // **JUNCTION CAP**
    Rectangle {
        id: junctionCap

        width: junctionCapRadius * 2
        height: junctionCapRadius * 2
        radius: junctionCapRadius

        x: {
            var junctionPx = getJunctionPixel();
            return junctionPx.x - pointMachine.x - junctionCapRadius;
        }
        y: {
            var junctionPx = getJunctionPixel();
            return junctionPx.y - pointMachine.y - junctionCapRadius;
        }

        color: junctionCapColor
        z: 1

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

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: railLineColor
            border.width: 1
            radius: parent.radius
            opacity: 0.6
        }
    }

    // **MOTOR INDICATOR**
    Rectangle {
        id: motorIndicator
        width: cellSize * motorSizeMultiplier
        height: cellSize * motorSizeMultiplier
        radius: width / 2
        color: getMotorColor()
        border.color: motorBorderColor
        border.width: motorBorderWidth
        anchors.centerIn: parent
        z: 2

        // ✅ NEW: Lock indicator overlay
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.8
            height: parent.height * 0.8
            radius: width / 2
            color: "transparent"
            border.color: "#ff0000"
            border.width: 3
            visible: isLocked

            Text {
                anchors.centerIn: parent
                text: "🔒"
                color: "#ff0000"
                font.pixelSize: parent.width * 0.4
            }
        }

        Rectangle {
            width: parent.width * motorInnerSizeMultiplier
            height: parent.height * motorInnerSizeMultiplier
            radius: width / 2
            color: motorInnerColor
            anchors.centerIn: parent
            rotation: position === "NORMAL" ? normalPositionAngle : reversePositionAngle
            visible: !isLocked

            Behavior on rotation {
                NumberAnimation {
                    duration: operatingStatus === "IN_TRANSITION" ? normalAnimationDuration : quickAnimationDuration
                    easing.type: Easing.OutCubic
                }
            }
        }

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

    // ✅ NEW: Machine name label
    // Text {
    //     anchors.bottom: parent.bottom
    //     anchors.horizontalCenter: parent.horizontalCenter
    //     anchors.bottomMargin: -20
    //     text: machineName || machineId
    //     color: "#ffffff"
    //     font.pixelSize: 10
    //     font.family: "Arial"
    //     horizontalAlignment: Text.AlignHCenter
    //     visible: machineName.length > 0
    // }

    // **CLICKABLE AREA**
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: isLocked ? Qt.ForbiddenCursor : Qt.PointingHandCursor
        enabled: !isLocked

        onClicked: {
            console.log("Point machine clicked:", machineId, "Current position:", position, "Status:", operatingStatus, "Locked:", isLocked)
            if (!isLocked) {
                pointMachine.pointMachineClicked(machineId, position)
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "white"
            opacity: parent.containsMouse && !isLocked ? hoverOpacity : 0
            radius: hoverRadius

            Behavior on opacity {
                NumberAnimation { duration: quickAnimationDuration }
            }
        }
    }
}
