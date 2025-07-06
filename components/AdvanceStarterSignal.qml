// components/AdvanceStarterSignal.qml
import QtQuick
import "../data/StationData.js" as StationData

Item {
    id: advanceStarterSignal

    property string signalId: ""
    property string signalName: ""
    property string currentAspect: {
        var signal = StationData.getAdvanceStarterSignalById(signalId)
        return signal ? signal.currentAspect : "RED"
    }
    property string direction: "UP"
    property bool isActive: true
    property int cellSize: 20

    // ============================================================================
    // PARENT DIMENSIONS - Master sizing control
    // ============================================================================
    property real scalingConstant: 15  // Smaller than outer signals since only 2 aspects
    property real scalingFactor: 0.46875
    property real parentWidth: cellSize * scalingConstant
    property real parentHeight: parentWidth * scalingFactor

    width: parentWidth
    height: parentHeight

    // ============================================================================
    // MAST GROUP VARIABLES
    // ============================================================================
    property real mastWidth: parentWidth * 0.03125
    property real mastHeight: parentHeight * 0.4

    // ============================================================================
    // ARM GROUP VARIABLES
    // ============================================================================
    property real armWidth: parentWidth * 0.1125
    property real armHeight: parentHeight * 0.053

    // ============================================================================
    // SIGNAL CIRCLE GROUP VARIABLES (Only 2 circles)
    // ============================================================================
    property real circleWidth: parentWidth * 0.15625  // Slightly larger since only 2 circles
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
    // SIGNAL COLOR GROUP (Only RED and GREEN for Advanced Starter)
    // ============================================================================
    property color redAspectColor: "#ff0000"
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

        // **SIGNAL CIRCLES** - Only RED and GREEN
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: circleSpacing

            // **RED CIRCLE** (Left)
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

            // **GREEN CIRCLE** (Right)
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

        // **SIGNAL CIRCLES** - Only RED and GREEN
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: circleSpacing

            // **RED CIRCLE** (Left when reversed)
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

            // **GREEN CIRCLE** (Right when reversed)
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

    // ============================================================================
    // INTERACTION
    // ============================================================================
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            console.log("Advanced starter signal clicked:", signalId, "Current aspect:", currentAspect, "Direction:", direction)
            advanceStarterSignal.signalClicked(signalId, currentAspect)
        }
    }
}
