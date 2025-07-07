// components/HomeSignal.qml
import QtQuick

Item {
    id: homeSignal

    // ============================================================================
    // COMPONENT PROPERTIES
    // ============================================================================
    property string signalId: ""
    property string signalName: ""
    property string currentAspect: "RED"
    property int aspectCount: 3                     // ✅ NEW: Number of aspects from database
    property var possibleAspects: []               // ✅ NEW: Valid aspects from database
    property string callingOnAspect: "OFF"         // "WHITE", "DARK", "OFF"
    property string loopAspect: "OFF"              // ✅ UPDATED: "YELLOW", "DARK", "INACTIVE"
    property string loopSignalConfiguration: "UR"
    property string direction: "UP"
    property bool isActive: true                   // ✅ NEW: Signal active status from database
    property string locationDescription: ""        // ✅ NEW: Location info from database (not rendered)
    property int cellSize: 20

    // ============================================================================
    // PARSED CONFIGURATION PROPERTIES
    // ============================================================================
    property string loopMastDirection: loopSignalConfiguration.charAt(0)  // "U" or "D"
    property string loopArmDirection: loopSignalConfiguration.charAt(1)   // "L" or "R"

    // ============================================================================
    // ✅ UPDATED: COMPUTED PROPERTIES WITH DATABASE SUPPORT
    // ============================================================================
    property bool isLoopSignalActive: loopAspect !== "INACTIVE"

    // ============================================================================
    // PARENT DIMENSIONS - Master sizing control
    // ============================================================================
    property real scalingConstant: 15
    property real scalingFactor: 0.46875
    property real parentWidth: cellSize * scalingConstant
    property real parentHeight: parentWidth * scalingFactor

    width: parentWidth
    height: parentHeight

    // ============================================================================
    // MAST GROUP VARIABLES - Two masts
    // ============================================================================
    property real mast1Width: parentWidth * 0.03125
    property real mast1Height: parentHeight * 0.4
    property real mast2Width: parentWidth * 0.025
    property real mast2Height: parentHeight * 0.43

    // ============================================================================
    // ARM GROUP VARIABLES - Three arm segments
    // ============================================================================
    property real armSegment1Width: parentWidth * 0.1125
    property real armSegment2Width: parentWidth * 0.1125
    property real armSegment3Width: parentWidth * 0.08
    property real armHeight: parentHeight * 0.053

    // ============================================================================
    // CALLING-ON SIGNAL VARIABLES
    // ============================================================================
    property real callingOnWidth: parentWidth * 0.15625 * 0.75
    property real callingOnHeight: parentHeight * 0.333 * 0.75

    // ============================================================================
    // LOOP SIGNAL VARIABLES
    // ============================================================================
    property real loopWidth: parentWidth * 0.15625 * 0.7
    property real loopHeight: parentHeight * 0.333 * 0.7

    // ============================================================================
    // MAIN SIGNAL CIRCLE GROUP VARIABLES
    // ============================================================================
    property real circleWidth: parentWidth * 0.15625
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
    property color armColorActive: "#ffffff"
    property color armColorInactive: "#b3b3b3"
    property color lampOffColor: "#404040"
    property color inactiveColor: "#606060"        // ✅ NEW: Color for inactive signals
    property color inactiveMastColor: "#888888"    // ✅ NEW: Dimmed mast for inactive signals

    // ============================================================================
    // SIGNAL COLOR GROUP
    // ============================================================================
    property color redAspectColor: "#ff0000"
    property color yellowAspectColor: "#ffff00"
    property color greenAspectColor: "#00ff00"
    property color callingOnColor: "#f0f8ff"       // White color for calling-on
    property color loopColor: "#ffff00"            // Yellow color for loop signal

    // ============================================================================
    // ✅ NEW: INACTIVE SIGNAL PROPERTIES
    // ============================================================================
    readonly property real inactiveOpacity: 0.5
    readonly property color inactiveBorderColor: "#ff6600"
    readonly property real inactiveBorderWidth: 2

    // ============================================================================
    // ✅ ENHANCED: DYNAMIC COLOR LOGIC
    // ============================================================================
    property color currentArmColor: {
        if (!isActive) return inactiveMastColor;
        return (callingOnAspect === "WHITE") ? armColorActive : armColorInactive;
    }

    function getMastColor() {
        return isActive ? mastColor : inactiveMastColor;
    }

    function getLoopSignalColor() {
        if (!isActive) return inactiveColor;

        switch(loopAspect) {
            case "YELLOW": return loopColor        // Yellow when active
            case "DARK": return lampOffColor       // Dark/off color
            case "INACTIVE": return lampOffColor   // Not visible anyway
            default: return lampOffColor           // Safe default
        }
    }

    function getCallingOnColor() {
        if (!isActive) return inactiveColor;
        return (callingOnAspect === "WHITE") ? callingOnColor : lampOffColor;
    }

    function getMainLampColor(aspectToCheck) {
        if (!isActive) return inactiveColor;
        return (currentAspect === aspectToCheck) ? getAspectColor(aspectToCheck) : lampOffColor;
    }

    function getAspectColor(aspect) {
        switch(aspect) {
            case "RED": return redAspectColor;
            case "YELLOW": return yellowAspectColor;
            case "GREEN": return greenAspectColor;
            default: return lampOffColor;
        }
    }

    // ============================================================================
    // ✅ NEW: DATABASE VALIDATION FUNCTIONS
    // ============================================================================
    function isValidAspect(aspect) {
        if (possibleAspects.length === 0) return true; // No restriction
        return possibleAspects.indexOf(aspect) !== -1;
    }

    function getAspectDisplayName(aspect) {
        switch(aspect) {
            case "RED": return "Danger"
            case "YELLOW": return "Caution"
            case "GREEN": return "Clear"
            default: return aspect
        }
    }

    function getSignalTypeDescription() {
        return "Home Signal (" + aspectCount + "-aspect)"
    }

    function isOperational() {
        return isActive && isValidAspect(currentAspect);
    }

    function getNextValidAspect() {
        if (possibleAspects.length === 0) return currentAspect;
        var currentIndex = possibleAspects.indexOf(currentAspect);
        var nextIndex = (currentIndex + 1) % possibleAspects.length;
        return possibleAspects[nextIndex];
    }

    signal signalClicked(string signalId, string currentAspect)

    // ============================================================================
    // UP SIGNAL LAYOUT: Dynamic loop configuration (RED, YELLOW, GREEN sequence)
    // ============================================================================
    Item {
        id: upSignalLayout
        visible: direction === "UP"
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        opacity: isActive ? 1.0 : inactiveOpacity  // ✅ NEW: Dimmed when inactive

        Row {
            id: mainHorizontalLine
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            // **STEP 1: MAST 1**
            Rectangle {
                id: upMast1
                width: mast1Width
                height: mast1Height
                color: getMastColor()
                anchors.verticalCenter: parent.verticalCenter
            }

            // **STEP 2: ARM SEGMENT 1**
            Rectangle {
                id: upArmSegment1
                width: armSegment1Width
                height: armHeight
                color: currentArmColor
                anchors.verticalCenter: parent.verticalCenter
            }

            // **STEP 3: CALLING-ON SIGNAL**
            Rectangle {
                id: upCallingOnSignal
                width: callingOnWidth
                height: callingOnHeight
                radius: width / 2
                color: getCallingOnColor()
                border.color: borderColor
                border.width: borderWidth
                anchors.verticalCenter: parent.verticalCenter
            }

            // **STEP 4: ARM SEGMENT 2** (contains dynamic mast 2) - ✅ CONDITIONAL
            Item {
                id: upArmSegment2Container
                width: armSegment2Width
                height: armHeight
                anchors.verticalCenter: parent.verticalCenter
                visible: isLoopSignalActive  // ✅ Hide when loop is inactive

                // Arm segment 2 background
                Rectangle {
                    id: upArmSegment2
                    anchors.fill: parent
                    color: currentArmColor
                }

                // **DYNAMIC MAST 2** - ✅ CONDITIONAL Position based on loopMastDirection
                Rectangle {
                    id: upMast2
                    width: mast2Width
                    height: mast2Height
                    color: getMastColor()
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: isLoopSignalActive

                    // ✅ DYNAMIC MAST POSITIONING
                    anchors.bottom: loopMastDirection === "U" ? parent.top : undefined
                    anchors.top: loopMastDirection === "D" ? parent.bottom : undefined
                }

                // **DYNAMIC ARM SEGMENT 3** - ✅ CONDITIONAL Direction based on loopArmDirection
                Rectangle {
                    id: upArmSegment3
                    width: armSegment3Width
                    height: armHeight
                    color: currentArmColor
                    visible: isLoopSignalActive

                    // ✅ DYNAMIC ARM POSITIONING
                    anchors.left: loopArmDirection === "R" ? upMast2.right : undefined
                    anchors.right: loopArmDirection === "L" ? upMast2.left : undefined
                    anchors.verticalCenter: loopMastDirection === "U" ? upMast2.top : upMast2.bottom
                }

                // **DYNAMIC LOOP SIGNAL** - ✅ CONDITIONAL Position based on arm direction
                Rectangle {
                    id: upLoopSignal
                    width: loopWidth
                    height: loopHeight
                    radius: width / 2
                    color: getLoopSignalColor()
                    border.color: borderColor
                    border.width: borderWidth
                    visible: isLoopSignalActive

                    // ✅ DYNAMIC LOOP POSITIONING
                    anchors.left: loopArmDirection === "R" ? upArmSegment3.right : undefined
                    anchors.right: loopArmDirection === "L" ? upArmSegment3.left : undefined
                    anchors.verticalCenter: upArmSegment3.verticalCenter
                }
            }

            // **STEP 5: MAIN HOME SIGNAL CIRCLES - UP SEQUENCE: RED, YELLOW, GREEN**
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: circleSpacing

                // **UP LAMP 1: RED (Left)**
                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: getMainLampColor("RED")
                    border.color: borderColor
                    border.width: borderWidth
                }

                // **UP LAMP 2: YELLOW (Center)**
                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: getMainLampColor("YELLOW")
                    border.color: borderColor
                    border.width: borderWidth
                    visible: aspectCount >= 2  // ✅ NEW: Only show if signal supports it
                }

                // **UP LAMP 3: GREEN (Right)**
                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: getMainLampColor("GREEN")
                    border.color: borderColor
                    border.width: borderWidth
                    visible: aspectCount >= 3  // ✅ NEW: Only show if signal supports it
                }
            }
        }
    }

    // ============================================================================
    // DOWN SIGNAL LAYOUT: Dynamic loop configuration (GREEN, YELLOW, RED sequence)
    // ============================================================================
    Item {
        id: downSignalLayout
        visible: direction === "DOWN"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        opacity: isActive ? 1.0 : inactiveOpacity  // ✅ NEW: Dimmed when inactive

        Row {
            id: downMainHorizontalLine
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            layoutDirection: Qt.RightToLeft
            spacing: 0

            // **STEP 1: MAST 1 (rightmost)**
            Rectangle {
                id: downMast1
                width: mast1Width
                height: mast1Height
                color: getMastColor()
                anchors.verticalCenter: parent.verticalCenter
            }

            // **STEP 2: ARM SEGMENT 1**
            Rectangle {
                id: downArmSegment1
                width: armSegment1Width
                height: armHeight
                color: currentArmColor
                anchors.verticalCenter: parent.verticalCenter
            }

            // **STEP 3: CALLING-ON SIGNAL**
            Rectangle {
                id: downCallingOnSignal
                width: callingOnWidth
                height: callingOnHeight
                radius: width / 2
                color: getCallingOnColor()
                border.color: borderColor
                border.width: borderWidth
                anchors.verticalCenter: parent.verticalCenter
            }

            // **STEP 4: ARM SEGMENT 2** (contains dynamic mast 2) - ✅ CONDITIONAL
            Item {
                id: downArmSegment2Container
                width: armSegment2Width
                height: armHeight
                anchors.verticalCenter: parent.verticalCenter
                visible: isLoopSignalActive  // ✅ Hide when loop is inactive

                // Arm segment 2 background
                Rectangle {
                    id: downArmSegment2
                    anchors.fill: parent
                    color: currentArmColor
                }

                // **DYNAMIC MAST 2** - ✅ CONDITIONAL Position based on loopMastDirection
                Rectangle {
                    id: downMast2
                    width: mast2Width
                    height: mast2Height
                    color: getMastColor()
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: isLoopSignalActive

                    // ✅ DYNAMIC MAST POSITIONING
                    anchors.bottom: loopMastDirection === "U" ? parent.top : undefined
                    anchors.top: loopMastDirection === "D" ? parent.bottom : undefined
                }

                // **DYNAMIC ARM SEGMENT 3** - ✅ CONDITIONAL Direction based on loopArmDirection (inverted for DOWN)
                Rectangle {
                    id: downArmSegment3
                    width: armSegment3Width
                    height: armHeight
                    color: currentArmColor
                    visible: isLoopSignalActive

                    // ✅ DYNAMIC ARM POSITIONING (inverted logic for DOWN direction)
                    anchors.left: loopArmDirection === "L" ? downMast2.right : undefined
                    anchors.right: loopArmDirection === "R" ? downMast2.left : undefined
                    anchors.verticalCenter: loopMastDirection === "U" ? downMast2.top : downMast2.bottom
                }

                // **DYNAMIC LOOP SIGNAL** - ✅ CONDITIONAL Position based on arm direction (inverted for DOWN)
                Rectangle {
                    id: downLoopSignal
                    width: loopWidth
                    height: loopHeight
                    radius: width / 2
                    color: getLoopSignalColor()
                    border.color: borderColor
                    border.width: borderWidth
                    visible: isLoopSignalActive

                    // ✅ DYNAMIC LOOP POSITIONING (inverted logic for DOWN direction)
                    anchors.left: loopArmDirection === "L" ? downArmSegment3.right : undefined
                    anchors.right: loopArmDirection === "R" ? downArmSegment3.left : undefined
                    anchors.verticalCenter: downArmSegment3.verticalCenter
                }
            }

            // **STEP 5: MAIN HOME SIGNAL CIRCLES - DOWN SEQUENCE: GREEN, YELLOW, RED**
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: circleSpacing

                // **DOWN LAMP 1: GREEN (Left when mirrored = rightmost visually)**
                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: getMainLampColor("GREEN")
                    border.color: borderColor
                    border.width: borderWidth
                    visible: aspectCount >= 3  // ✅ NEW: Only show if signal supports it
                }

                // **DOWN LAMP 2: YELLOW (Center when mirrored)**
                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: getMainLampColor("YELLOW")
                    border.color: borderColor
                    border.width: borderWidth
                    visible: aspectCount >= 2  // ✅ NEW: Only show if signal supports it
                }

                // **DOWN LAMP 3: RED (Right when mirrored = leftmost visually)**
                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: getMainLampColor("RED")
                    border.color: borderColor
                    border.width: borderWidth
                }
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

        // "X" pattern for inactive signals
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
                console.log("Home signal operation blocked:", signalId,
                           "Active:", isActive,
                           "Valid aspect:", isValidAspect(currentAspect))
                return
            }

            console.log("Home signal clicked:", signalId,
                       "Name:", signalName || "Unnamed",
                       "Main aspect:", currentAspect, "(" + getAspectDisplayName(currentAspect) + ")",
                       "Calling-on:", callingOnAspect,
                       "Loop:", loopAspect,
                       "Loop config:", loopSignalConfiguration,
                       "Loop active:", isLoopSignalActive,
                       "Direction:", direction,
                       "Type:", getSignalTypeDescription(),
                       "Valid aspects:", possibleAspects,
                       "Next valid aspect:", getNextValidAspect())
            homeSignal.signalClicked(signalId, currentAspect)
        }

        // ✅ ENHANCED: HOVER EFFECT WITH STATUS INDICATION
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
