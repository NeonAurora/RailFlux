// components/HomeSignal.qml
import QtQuick
import "../data/StationData.js" as StationData

Item {
    id: homeSignal

    property string signalId: ""
    property string signalName: ""
    property string currentAspect: {
        var signal = StationData.getHomeSignalById(signalId)
        return signal ? signal.currentAspect : "RED"
    }

    // **CALLING-ON SIGNAL PROPERTY**
    property string callingOnAspect: {
        var signal = StationData.getHomeSignalById(signalId)
        return signal ? signal.callingOnAspect : "OFF"
    }

    // **LOOP SIGNAL PROPERTY**
    property string loopAspect: {
        var signal = StationData.getHomeSignalById(signalId)
        return signal ? signal.loopAspect : "OFF"
    }

    // **NEW: LOOP SIGNAL CONFIGURATION PROPERTY**
    property string loopSignalConfiguration: {
        var signal = StationData.getHomeSignalById(signalId)
        return signal ? signal.loopSignalConfiguration : "UR"  // Default: Up + Right
    }

    // **PARSED CONFIGURATION PROPERTIES**
    property string loopMastDirection: loopSignalConfiguration.charAt(0)  // "U" or "D"
    property string loopArmDirection: loopSignalConfiguration.charAt(1)   // "L" or "R"

    property string direction: "UP"
    property bool isActive: true
    property int cellSize: 20

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
    // MAST GROUP VARIABLES - Two masts now
    // ============================================================================
    property real mast1Width: parentWidth * 0.03125
    property real mast1Height: parentHeight * 0.4

    property real mast2Width: parentWidth * 0.025
    property real mast2Height: parentHeight * 0.43

    // ============================================================================
    // ARM GROUP VARIABLES - Three arm segments now
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
    // BASE COLOR GROUP
    // ============================================================================
    property color mastColor: "#ffffff"
    property color armColorActive: "#ffffff"
    property color armColorInactive: "#b3b3b3"
    property color lampOffColor: "#404040"

    // ============================================================================
    // SIGNAL COLOR GROUP
    // ============================================================================
    property color redAspectColor: "#ff0000"
    property color yellowAspectColor: "#ffff00"
    property color greenAspectColor: "#00ff00"
    property color callingOnColor: "#f0f8ff"
    property color loopColor: "#ffff00"

    // ============================================================================
    // DYNAMIC ARM COLOR
    // ============================================================================
    property color currentArmColor: {
        return (callingOnAspect === "WHITE") ? armColorActive : armColorInactive
    }

    signal signalClicked(string signalId, string currentAspect)

    // ============================================================================
    // UP SIGNAL LAYOUT: Dynamic loop configuration
    // ============================================================================
    Item {
        id: upSignalLayout
        visible: direction === "UP"
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter

        Row {
            id: mainHorizontalLine
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            // **STEP 1: MAST 1**
            Rectangle {
                id: upMast1
                width: mast1Width
                height: mast1Height
                color: mastColor
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
                color: {
                    if (callingOnAspect === "WHITE") return callingOnColor
                    return lampOffColor
                }
                border.color: borderColor
                border.width: borderWidth
                anchors.verticalCenter: parent.verticalCenter
            }

            // **STEP 4: ARM SEGMENT 2** (contains dynamic mast 2)
            Item {
                id: upArmSegment2Container
                width: armSegment2Width
                height: armHeight
                anchors.verticalCenter: parent.verticalCenter

                // Arm segment 2 background
                Rectangle {
                    id: upArmSegment2
                    anchors.fill: parent
                    color: currentArmColor
                }

                // **DYNAMIC MAST 2** - Position based on loopMastDirection
                Rectangle {
                    id: upMast2
                    width: mast2Width
                    height: mast2Height
                    color: mastColor
                    anchors.horizontalCenter: parent.horizontalCenter

                    // ✅ DYNAMIC MAST POSITIONING
                    anchors.bottom: loopMastDirection === "U" ? parent.top : undefined
                    anchors.top: loopMastDirection === "D" ? parent.bottom : undefined
                }

                // **DYNAMIC ARM SEGMENT 3** - Direction based on loopArmDirection
                Rectangle {
                    id: upArmSegment3
                    width: armSegment3Width
                    height: armHeight
                    color: currentArmColor

                    // ✅ DYNAMIC ARM POSITIONING
                    anchors.left: loopArmDirection === "R" ? upMast2.right : undefined
                    anchors.right: loopArmDirection === "L" ? upMast2.left : undefined
                    anchors.verticalCenter: loopMastDirection === "U" ? upMast2.top : upMast2.bottom
                }

                // **DYNAMIC LOOP SIGNAL** - Position based on arm direction
                Rectangle {
                    id: upLoopSignal
                    width: loopWidth
                    height: loopHeight
                    radius: width / 2
                    color: {
                        if (loopAspect === "YELLOW") return loopColor
                        return lampOffColor
                    }
                    border.color: borderColor
                    border.width: borderWidth

                    // ✅ DYNAMIC LOOP POSITIONING
                    anchors.left: loopArmDirection === "R" ? upArmSegment3.right : undefined
                    anchors.right: loopArmDirection === "L" ? upArmSegment3.left : undefined
                    anchors.verticalCenter: upArmSegment3.verticalCenter
                }
            }

            // **STEP 5: MAIN HOME SIGNAL CIRCLES**
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: circleSpacing

                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: {
                        if (currentAspect === "RED") return redAspectColor
                        return lampOffColor
                    }
                    border.color: borderColor
                    border.width: borderWidth
                }

                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: {
                        if (currentAspect === "YELLOW") return yellowAspectColor
                        return lampOffColor
                    }
                    border.color: borderColor
                    border.width: borderWidth
                }

                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: {
                        if (currentAspect === "GREEN") return greenAspectColor
                        return lampOffColor
                    }
                    border.color: borderColor
                    border.width: borderWidth
                }
            }
        }
    }

    // ============================================================================
    // DOWN SIGNAL LAYOUT: Dynamic loop configuration (mirrored)
    // ============================================================================
    Item {
        id: downSignalLayout
        visible: direction === "DOWN"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

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
                color: mastColor
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
                color: {
                    if (callingOnAspect === "WHITE") return callingOnColor
                    return lampOffColor
                }
                border.color: borderColor
                border.width: borderWidth
                anchors.verticalCenter: parent.verticalCenter
            }

            // **STEP 4: ARM SEGMENT 2** (contains dynamic mast 2)
            Item {
                id: downArmSegment2Container
                width: armSegment2Width
                height: armHeight
                anchors.verticalCenter: parent.verticalCenter

                // Arm segment 2 background
                Rectangle {
                    id: downArmSegment2
                    anchors.fill: parent
                    color: currentArmColor
                }

                // **DYNAMIC MAST 2** - Position based on loopMastDirection
                Rectangle {
                    id: downMast2
                    width: mast2Width
                    height: mast2Height
                    color: mastColor
                    anchors.horizontalCenter: parent.horizontalCenter

                    // ✅ DYNAMIC MAST POSITIONING
                    anchors.bottom: loopMastDirection === "U" ? parent.top : undefined
                    anchors.top: loopMastDirection === "D" ? parent.bottom : undefined
                }

                // **DYNAMIC ARM SEGMENT 3** - Direction based on loopArmDirection (inverted for DOWN)
                Rectangle {
                    id: downArmSegment3
                    width: armSegment3Width
                    height: armHeight
                    color: currentArmColor

                    // ✅ DYNAMIC ARM POSITIONING (inverted logic for DOWN direction)
                    anchors.left: loopArmDirection === "L" ? downMast2.right : undefined
                    anchors.right: loopArmDirection === "R" ? downMast2.left : undefined
                    anchors.verticalCenter: loopMastDirection === "U" ? downMast2.top : downMast2.bottom
                }

                // **DYNAMIC LOOP SIGNAL** - Position based on arm direction (inverted for DOWN)
                Rectangle {
                    id: downLoopSignal
                    width: loopWidth
                    height: loopHeight
                    radius: width / 2
                    color: {
                        if (loopAspect === "YELLOW") return loopColor
                        return lampOffColor
                    }
                    border.color: borderColor
                    border.width: borderWidth

                    // ✅ DYNAMIC LOOP POSITIONING (inverted logic for DOWN direction)
                    anchors.left: loopArmDirection === "L" ? downArmSegment3.right : undefined
                    anchors.right: loopArmDirection === "R" ? downArmSegment3.left : undefined
                    anchors.verticalCenter: downArmSegment3.verticalCenter
                }
            }

            // **STEP 5: MAIN HOME SIGNAL CIRCLES (leftmost)**
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: circleSpacing

                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: {
                        if (currentAspect === "RED") return redAspectColor
                        return lampOffColor
                    }
                    border.color: borderColor
                    border.width: borderWidth
                }

                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: {
                        if (currentAspect === "YELLOW") return yellowAspectColor
                        return lampOffColor
                    }
                    border.color: borderColor
                    border.width: borderWidth
                }

                Rectangle {
                    width: circleWidth
                    height: circleHeight
                    radius: width / 2
                    color: {
                        if (currentAspect === "GREEN") return greenAspectColor
                        return lampOffColor
                    }
                    border.color: borderColor
                    border.width: borderWidth
                }
            }
        }
    }

    // ============================================================================
    // INTERACTION
    // ============================================================================
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            console.log("Home signal clicked:", signalId, "Main:", currentAspect, "Calling-on:", callingOnAspect, "Loop:", loopAspect, "Config:", loopSignalConfiguration)
            homeSignal.signalClicked(signalId, currentAspect)
        }
    }
}
