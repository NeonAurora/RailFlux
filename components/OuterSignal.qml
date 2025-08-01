// components/OuterSignal.qml
import QtQuick

Item {
    id: outerSignal

    // ============================================================================
    // COMPONENT PROPERTIES (unchanged)
    // ============================================================================
    property string signalId: ""
    property string signalName: ""
    property string currentAspect: "RED"
    property int aspectCount: 4
    property var possibleAspects: []
    property string direction: "UP"
    property bool isActive: true
    property string locationDescription: ""
    property int cellSize: 20

    // ============================================================================
    // DIMENSIONS AND COLORS (unchanged)
    // ============================================================================
    property real scalingConstant: 15
    property real scalingFactor: 0.46875
    property real parentWidth: cellSize * scalingConstant
    property real parentHeight: parentWidth * scalingFactor

    width: parentWidth
    height: parentHeight

    property real mastWidth: parentWidth * 0.03125
    property real mastHeight: parentHeight * 0.4
    property real armWidth: parentWidth * 0.1125
    property real armHeight: parentHeight * 0.053
    property real circleWidth: parentWidth * 0.15625
    property real circleHeight: parentHeight * 0.333
    property real circleSpacing: 0
    property real borderWidth: 0.5
    property color borderColor: "#b3b3b3"

    property color mastColor: "#ffffff"
    property color armColor: "#ffffff"
    property color lampOffColor: "#404040"
    property color inactiveColor: "#606060"
    property color inactiveMastColor: "#888888"

    property color redAspectColor: "#ff0000"
    property color yellowAspectColor: "#ffff00"
    property color greenAspectColor: "#00ff00"
    property color blueAspectColor: "#3182ce"
    property color whiteAspectColor: "#ffffff"

    readonly property real inactiveOpacity: 0.5
    readonly property color inactiveBorderColor: "#ff6600"
    readonly property real inactiveBorderWidth: 2

    signal signalClicked(string signalId, string currentAspect)

    // ============================================================================
    // DATABASE VALIDATION FUNCTIONS (unchanged)
    // ============================================================================
    function isValidAspect(aspect) {
        if (possibleAspects.length === 0) return true;
        return possibleAspects.indexOf(aspect) !== -1;
    }

    function isOperational() {
        return isActive && isValidAspect(currentAspect);
    }

    function getMastColor() {
        return isActive ? mastColor : inactiveMastColor;
    }

    function getArmColor() {
        return isActive ? armColor : inactiveMastColor;
    }

    // ============================================================================
    // ✅ FIXED: CORRECTED LAMP COLOR LOGIC
    // ============================================================================
    function getLampColor(aspectToCheck) {
        if (!isActive) return inactiveColor;

        switch(aspectToCheck) {
            case "RED":
                return currentAspect === "RED" ? redAspectColor : lampOffColor;
            case "SINGLE_YELLOW":
                return (currentAspect === "SINGLE_YELLOW" || currentAspect === "DOUBLE_YELLOW") ? yellowAspectColor : lampOffColor;
            case "DOUBLE_YELLOW":
                return currentAspect === "DOUBLE_YELLOW" ? yellowAspectColor : lampOffColor;
            case "GREEN":
                return currentAspect === "GREEN" ? greenAspectColor : lampOffColor;
            default:
                return lampOffColor;
        }
    }

    // ============================================================================
    // ✅ FIXED: UP SIGNAL LAYOUT (YELLOW, RED, YELLOW, GREEN)
    // ============================================================================
    Row {
        id: upSignalLayout
        visible: direction === "UP"
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0
        opacity: isActive ? 1.0 : inactiveOpacity

        Rectangle {
            id: upMast
            width: mastWidth
            height: mastHeight
            color: getMastColor()
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            id: upArm
            width: armWidth
            height: armHeight
            color: getArmColor()
            anchors.verticalCenter: parent.verticalCenter
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: circleSpacing

            // **✅ FIXED: UP LAMP 1: YELLOW (Left) - Single or Double Yellow (PRIMARY YELLOW - closest to arm)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("SINGLE_YELLOW")  // ✅ FIXED: Now lights for both Single and Double
                border.color: borderColor
                border.width: borderWidth
                visible: aspectCount >= 3
            }

            // **UP LAMP 2: RED (Center-Left)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("RED")
                border.color: borderColor
                border.width: borderWidth
            }

            // **✅ FIXED: UP LAMP 3: YELLOW (Center-Right) - Double Yellow Only (SECONDARY YELLOW - near green)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("DOUBLE_YELLOW")  // ✅ FIXED: Now only lights for Double Yellow
                border.color: borderColor
                border.width: borderWidth
                visible: aspectCount >= 4
            }

            // **UP LAMP 4: GREEN (Right)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("GREEN")
                border.color: borderColor
                border.width: borderWidth
                visible: aspectCount >= 3
            }
        }
    }

    // ============================================================================
    // ✅ FIXED: DOWN SIGNAL LAYOUT (GREEN, YELLOW, RED, YELLOW)
    // ============================================================================
    Row {
        id: downSignalLayout
        visible: direction === "DOWN"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        layoutDirection: Qt.RightToLeft
        spacing: 0
        opacity: isActive ? 1.0 : inactiveOpacity

        Rectangle {
            id: downMast
            width: mastWidth
            height: mastHeight
            color: getMastColor()
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            id: downArm
            width: armWidth
            height: armHeight
            color: getArmColor()
            anchors.verticalCenter: parent.verticalCenter
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: circleSpacing

            // **DOWN LAMP 1: GREEN (Left when mirrored = rightmost visually)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("GREEN")
                border.color: borderColor
                border.width: borderWidth
                visible: aspectCount >= 3
            }

            // **✅ FIXED: DOWN LAMP 2: YELLOW (Center-Left when mirrored) - Double Yellow Only (SECONDARY YELLOW)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("DOUBLE_YELLOW")  // ✅ FIXED: Only for Double Yellow
                border.color: borderColor
                border.width: borderWidth
                visible: aspectCount >= 4
            }

            // **DOWN LAMP 3: RED (Center-Right when mirrored)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("RED")
                border.color: borderColor
                border.width: borderWidth
            }

            // **✅ FIXED: DOWN LAMP 4: YELLOW (Right when mirrored = leftmost visually) - Single or Double Yellow (PRIMARY YELLOW)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("SINGLE_YELLOW")  // ✅ FIXED: Lights for both Single and Double
                border.color: borderColor
                border.width: borderWidth
                visible: aspectCount >= 3
            }
        }
    }

    // ============================================================================
    // INACTIVE SIGNAL OVERLAY AND INTERACTION (unchanged)
    // ============================================================================
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: inactiveBorderColor
        border.width: inactiveBorderWidth
        radius: 4
        visible: !isActive
        opacity: 0.7

        Rectangle {
            width: parent.width * 1.414
            height: 1
            color: inactiveBorderColor
            anchors.centerIn: parent
            rotation: 45
            opacity: 0.6
        }
        Rectangle {
            width: parent.width * 1.414
            height: 1
            color: inactiveBorderColor
            anchors.centerIn: parent
            rotation: -45
            opacity: 0.6
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: isOperational() ? Qt.PointingHandCursor : Qt.ForbiddenCursor

        onClicked: {
            if (!isOperational()) {
                console.log("Outer signal operation blocked:", signalId, "Active:", isActive)
                return
            }

            console.log("Outer signal clicked:", signalId, "Current aspect:", currentAspect, "Direction:", direction)
            outerSignal.signalClicked(signalId, currentAspect)
        }

        Rectangle {
            anchors.fill: parent
            color: isOperational() ? "white" : "red"
            opacity: parent.containsMouse ? 0.1 : 0
            radius: 4

            Behavior on opacity {
                NumberAnimation { duration: 150 }
            }
        }
    }
}
