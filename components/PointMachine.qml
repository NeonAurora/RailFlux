// components/PointMachine.qml
import QtQuick
import "../data/StationData.js" as StationData

Item {
    id: pointMachine

    property string machineId: ""
    property string position: "NORMAL"              // "NORMAL" or "REVERSE"
    property string operatingStatus: "CONNECTED"    // "CONNECTED" or "IN_TRANSITION"
    property var junctionPoint: ({ row: 0, col: 0 })
    property var rootTrack: ({})
    property var normalTrack: ({})
    property var reverseTrack: ({})
    property int cellSize: 20
    property int transitionTime: 3000

    // Position at junction point
    x: junctionPoint.col * cellSize - width / 2
    y: junctionPoint.row * cellSize - height / 2
    width: cellSize * 10
    height: cellSize * 10

    signal pointMachineClicked(string machineId, string currentPosition)

    // **CONNECTION RENDERING**
    Canvas {
        id: connectionCanvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            // Junction center point
            var junctionX = width / 2;
            var junctionY = height / 2;

            // ✅ FIXED: Set line properties to match track segments
            ctx.lineCap = "butt";      // Square ends like rectangles
            ctx.lineJoin = "miter";    // Sharp corners

            // Get track endpoints with offset support
            var rootEndpoint = getTrackEndpointInCanvas(rootTrack);

            // Determine which track is active and get its endpoint
            var activeTrackInfo = (position === "NORMAL") ? normalTrack : reverseTrack;
            var activeEndpoint = getTrackEndpointInCanvas(activeTrackInfo);

            // Draw root connection (always active) - 8px to match track segments
            drawConnection(ctx, rootEndpoint, { x: junctionX, y: junctionY }, "#00aa00", 8);

            // ✅ FIXED: Draw only the ACTIVE path - no inactive lines
            drawConnection(ctx, { x: junctionX, y: junctionY }, activeEndpoint, getActiveColor(), 8);
        }

        // Repaint when properties change
        Component.onCompleted: {
            pointMachine.positionChanged.connect(requestPaint);
            pointMachine.operatingStatusChanged.connect(requestPaint);
        }
    }

    // **MOTOR INDICATOR**
    Rectangle {
        id: motorIndicator
        width: cellSize * 0.5
        height: cellSize * 0.5
        radius: width / 2
        color: getMotorColor()
        border.color: "#ffffff"
        border.width: 1
        anchors.centerIn: parent

        // Position indicator
        Rectangle {
            width: parent.width * 0.5
            height: parent.height * 0.5
            radius: width / 2
            color: "#ffffff"
            anchors.centerIn: parent
            rotation: position === "NORMAL" ? 0 : 45

            Behavior on rotation {
                NumberAnimation {
                    duration: operatingStatus === "IN_TRANSITION" ? 300 : 150
                }
            }
        }

        // Status text
        Text {
            anchors.top: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 4
            text: position + (operatingStatus === "IN_TRANSITION" ? " (MOVING)" : "")
            color: getMotorColor()
            font.pixelSize: 7
            font.bold: true
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

        // Hover effect
        Rectangle {
            anchors.fill: parent
            color: "white"
            opacity: parent.containsMouse ? 0.1 : 0
            radius: 6

            Behavior on opacity {
                NumberAnimation { duration: 150 }
            }
        }
    }

    // **HELPER FUNCTIONS**
    function drawConnection(ctx, from, to, color, lineWidth) {
        ctx.strokeStyle = color;
        ctx.lineWidth = lineWidth;

        // Set line style to match track segments
        ctx.lineCap = "butt";      // Square ends like rectangles
        ctx.lineJoin = "miter";    // Sharp corners

        ctx.beginPath();
        ctx.moveTo(from.x, from.y);
        ctx.lineTo(to.x, to.y);
        ctx.stroke();
    }

    function getTrackEndpointInCanvas(trackInfo) {
        var track = StationData.getTrackById(trackInfo.trackId);
        if (!track) return { x: width / 2, y: height / 2 };  // Default to junction center

        // Get the base endpoint from track segment
        var endpoint = StationData.getTrackEndpoint(track, trackInfo.connectionEnd);

        // ✅ NEW: Apply offset if specified
        var offsetRow = trackInfo.offset ? (trackInfo.offset.row || 0) : 0;
        var offsetCol = trackInfo.offset ? (trackInfo.offset.col || 0) : 0;

        // Apply offset to the endpoint
        var adjustedEndpoint = {
            row: endpoint.row + offsetRow,
            col: endpoint.col + offsetCol
        };

        // Convert to canvas-relative coordinates
        return {
            x: (adjustedEndpoint.col * cellSize) - pointMachine.x,
            y: (adjustedEndpoint.row * cellSize) - pointMachine.y
        };
    }

    function getActiveColor() {
        switch(operatingStatus) {
            case "CONNECTED":
                return position === "NORMAL" ? "#00ff00" : "#ffaa00";
            case "IN_TRANSITION":
                return "#ff6600";
            default:
                return "#aa0000";
        }
    }

    function getMotorColor() {
        switch(operatingStatus) {
            case "CONNECTED": return "#2d3748";
            case "IN_TRANSITION": return "#ff6600";
            default: return "#aa0000";
        }
    }
}
