// components/StarterSignal.qml
import QtQuick

Item {
    id: starterSignal

    // ============================================================================
    // COMPONENT PROPERTIES
    // ============================================================================
    property string signalId: ""
    property string signalName: ""
    property string currentAspect: "RED"
    property int aspectCount: 2                     // ✅ 2 or 3-aspect from database
    property var possibleAspects: []               // ✅ NEW: Valid aspects from database
    property string direction: "UP"
    property bool isActive: true                   // ✅ NEW: Signal active status from database
    property string locationDescription: ""        // ✅ NEW: Location info from database (not rendered)
    property int cellSize: 20

    // ============================================================================
    // PARENT DIMENSIONS - Master sizing control
    // ============================================================================
    property real scalingConstant: aspectCount === 2 ? 10 : 12  // Smaller for 2-aspect
    property real scalingFactor: 0.46875
    property real parentWidth: cellSize * scalingConstant
    property real parentHeight: parentWidth * scalingFactor

    width: parentWidth
    height: parentHeight

    // ============================================================================
    // MAST GROUP VARIABLES
    // ============================================================================
    property real mastWidth: parentWidth * 0.04
    property real mastHeight: parentHeight * 0.4

    // ============================================================================
    // ARM GROUP VARIABLES
    // ============================================================================
    property real armWidth: parentWidth * 0.15
    property real armHeight: parentHeight * 0.053

    // ============================================================================
    // SIGNAL CIRCLE GROUP VARIABLES
    // ============================================================================
    property real circleWidth: parentWidth * (aspectCount === 2 ? 0.16 : 0.16)
    property real circleHeight: parentHeight * 0.333
    property real circleSpacing: 0

    // ============================================================================
    // BORDER GROUP
    // ============================================================================
    property real borderWidth: 0.5
    property color borderColor: "#b3b3b3"

    // ============================================================================
    // ✅ ENHANCED: BASE COLOR GROUP WITH INACTIVE SUPPORT
    // ============================================================================
    property color mastColor: "#ffffff"
    property color armColor: "#ffffff"
    property color lampOffColor: "#404040"
    property color inactiveColor: "#606060"
    property color inactiveMastColor: "#888888"

    // ============================================================================
    // SIGNAL COLOR GROUP
    // ============================================================================
    property color redAspectColor: "#ff0000"
    property color yellowAspectColor: "#ffff00"
    property color greenAspectColor: "#00ff00"

    // ============================================================================
    // ✅ NEW: INACTIVE SIGNAL PROPERTIES
    // ============================================================================
    readonly property real inactiveOpacity: 0.5
    readonly property color inactiveBorderColor: "#ff6600"
    readonly property real inactiveBorderWidth: 2

    signal signalClicked(string signalId, string currentAspect)

    // ============================================================================
    // ✅ NEW: DATABASE VALIDATION FUNCTIONS
    // ============================================================================
    function isValidAspect(aspect) {
        if (possibleAspects.length === 0) return true;
        return possibleAspects.indexOf(aspect) !== -1;
    }

    function isOperational() {
        return isActive && isValidAspect(currentAspect);
    }

    // ============================================================================
    // ✅ NEW: ENHANCED COLOR FUNCTIONS WITH VALIDATION
    // ============================================================================
    function getMastColor() {
        return isActive ? mastColor : inactiveMastColor;
    }

    function getArmColor() {
        return isActive ? armColor : inactiveMastColor;
    }

    function getLampColor(aspectToCheck) {
        if (!isActive) return inactiveColor;

        switch(aspectToCheck) {
            case "RED":
                return currentAspect === "RED" ? redAspectColor : lampOffColor;
            case "YELLOW":
                return currentAspect === "YELLOW" ? yellowAspectColor : lampOffColor;
            case "GREEN":
                return currentAspect === "GREEN" ? greenAspectColor : lampOffColor;
            default:
                return lampOffColor;
        }
    }

    // ============================================================================
    // UP SIGNAL LAYOUT: mast → arm → circles (RED, YELLOW, GREEN)
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

        // **✅ UP SEQUENCE: RED (Left), YELLOW (Center), GREEN (Right)**
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: circleSpacing

            // **UP LAMP 1: RED (Left)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("RED")
                border.color: borderColor
                border.width: borderWidth
            }

            // **UP LAMP 2: YELLOW (Center)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("YELLOW")
                border.color: borderColor
                border.width: borderWidth
            }

            // **UP LAMP 3: GREEN (Right) - Only for 3-aspect**
            Rectangle {
                visible: aspectCount >= 3
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("GREEN")
                border.color: borderColor
                border.width: borderWidth
            }
        }
    }

    // ============================================================================
    // DOWN SIGNAL LAYOUT: circles → arm → mast (GREEN, YELLOW, RED) - MIRRORED
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

        // **✅ DOWN SEQUENCE: GREEN (Left visually), YELLOW (Center), RED (Right visually)**
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: circleSpacing

            // **DOWN LAMP 1: GREEN (Left visually due to RightToLeft)**
            Rectangle {
                visible: aspectCount >= 3
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("GREEN")
                border.color: borderColor
                border.width: borderWidth
            }

            // **DOWN LAMP 2: YELLOW (Center)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("YELLOW")
                border.color: borderColor
                border.width: borderWidth
            }

            // **DOWN LAMP 3: RED (Right visually due to RightToLeft)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: getLampColor("RED")
                border.color: borderColor
                border.width: borderWidth
            }
        }
    }

    // ✅ NEW: INACTIVE SIGNAL OVERLAY
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

    // ============================================================================
    // ✅ ENHANCED: INTERACTION WITH DATABASE VALIDATION
    // ============================================================================
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: isOperational() ? Qt.PointingHandCursor : Qt.ForbiddenCursor

        onClicked: {
            if (!isOperational()) {
                console.log("Starter signal operation blocked:", signalId, "Active:", isActive)
                return
            }

            console.log("Starter signal clicked:", signalId, "Current aspect:", currentAspect, "Direction:", direction, "Aspect count:", aspectCount)
            starterSignal.signalClicked(signalId, currentAspect)
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
