// components/StarterSignal.qml
import QtQuick
import "../data/StationData.js" as StationData

Item {
    id: starterSignal

    property string signalId: ""
    property string signalName: ""
    property string currentAspect: {
        var signal = StationData.getStarterSignalById(signalId)
        return signal ? signal.currentAspect : "RED"
    }

    property int aspectCount: {
        var signal = StationData.getStarterSignalById(signalId)
        return signal ? signal.aspectCount : 2  // Default to 2-aspect
    }

    property string direction: "UP"
    property bool isActive: true
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
    property real circleWidth: parentWidth * (aspectCount === 2 ? 0.16 : 0.16)  // Larger for 2-aspect
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
    property color armColor: "#ffffff"
    property color lampOffColor: "#404040"

    // ============================================================================
    // SIGNAL COLOR GROUP
    // ============================================================================
    property color redAspectColor: "#ff0000"
    property color yellowAspectColor: "#ffff00"
    property color greenAspectColor: "#00ff00"

    signal signalClicked(string signalId, string currentAspect)

    // ============================================================================
    // UP SIGNAL LAYOUT: mast → arm → circles
    // ============================================================================
    Row {
        id: upSignalLayout
        visible: direction === "UP"
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        // **MAST**
        Rectangle {
            id: upMast
            width: mastWidth
            height: mastHeight
            color: mastColor
            anchors.verticalCenter: parent.verticalCenter
        }

        // **ARM**
        Rectangle {
            id: upArm
            width: armWidth
            height: armHeight
            color: armColor
            anchors.verticalCenter: parent.verticalCenter
        }

        // **SIGNAL CIRCLES** - Dynamic count based on aspectCount
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: circleSpacing

            // **RED CIRCLE** - Always present
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

            // **YELLOW CIRCLE** - Always present
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

            // **GREEN CIRCLE** - Only for 3-aspect signals
            Rectangle {
                visible: aspectCount === 3
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

    // ============================================================================
    // DOWN SIGNAL LAYOUT: circles → arm → mast
    // ============================================================================
    Row {
        id: downSignalLayout
        visible: direction === "DOWN"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        layoutDirection: Qt.RightToLeft
        spacing: 0

        // **MAST**
        Rectangle {
            id: downMast
            width: mastWidth
            height: mastHeight
            color: mastColor
            anchors.verticalCenter: parent.verticalCenter
        }

        // **ARM**
        Rectangle {
            id: downArm
            width: armWidth
            height: armHeight
            color: armColor
            anchors.verticalCenter: parent.verticalCenter
        }

        // **SIGNAL CIRCLES** - Dynamic count based on aspectCount
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: circleSpacing

            // **RED CIRCLE** - Always present
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

            // **YELLOW CIRCLE** - Always present
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

            // **GREEN CIRCLE** - Only for 3-aspect signals
            Rectangle {
                visible: aspectCount === 3
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

    // ============================================================================
    // INTERACTION
    // ============================================================================
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            console.log("Starter signal clicked:", signalId, "Current aspect:", currentAspect, "Aspect count:", aspectCount, "Direction:", direction)
            starterSignal.signalClicked(signalId, currentAspect)
        }
    }
}
