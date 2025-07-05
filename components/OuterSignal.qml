// components/OuterSignal.qml
import QtQuick
import "../data/StationData.js" as StationData

Item {
    id: outerSignal

    property string signalId: ""
    property string signalName: ""
    property string currentAspect: {
        var signal = StationData.getOuterSignalById(signalId)
        return signal ? signal.currentAspect : "RED"
    }
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
    // MAST GROUP VARIABLES - Ratios based on parent dimensions
    // ============================================================================
    property real mastWidth: parentWidth * 0.03125    // 1/32 of parent width
    property real mastHeight: parentHeight * 0.4      // 80% of parent height

    // ============================================================================
    // ARM GROUP VARIABLES - Ratios based on parent dimensions
    // ============================================================================
    property real armWidth: parentWidth * 0.1125      // 10/32 of parent width
    property real armHeight: parentHeight * 0.053     // ~5% of parent height

    // ============================================================================
    // SIGNAL CIRCLE GROUP VARIABLES - Ratios based on parent dimensions
    // ============================================================================
    property real circleWidth: parentWidth * 0.15625  // 5/32 of parent width
    property real circleHeight: parentHeight * 0.333  // ~33% of parent height
    property real circleSpacing: 0                    // No spacing between circles

    // ============================================================================
    // BORDER GROUP - All border properties
    // ============================================================================
    property real borderWidth: 0.5
    property color borderColor: "#b3b3b3"

    // ============================================================================
    // BASE COLOR GROUP - Structural colors (mast, arm, backgrounds)
    // ============================================================================
    property color mastColor: "#ffffff"
    property color armColor: "#ffffff"
    property color lampOffColor: "#404040"    // Dark color when lamp is off

    // ============================================================================
    // SIGNAL COLOR GROUP - Aspect indication colors
    // ============================================================================
    property color redAspectColor: "#ff0000"      // Danger red
    property color yellowAspectColor: "#ffff00"   // Caution yellow
    property color greenAspectColor: "#00ff00"    // Clear green
    property color blueAspectColor: "#3182ce"     // Optional blue for other signals
    property color whiteAspectColor: "#ffffff"    // Optional white for shunt signals

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

        // **MAST GROUP**
        Rectangle {
            id: upMast
            width: mastWidth
            height: mastHeight
            color: mastColor
        }

        // **ARM GROUP**
        Rectangle {
            id: upArm
            width: armWidth
            height: armHeight
            color: armColor
            anchors.verticalCenter: parent.verticalCenter
        }

        // **SIGNAL CIRCLES**
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: circleSpacing

            // **LAMP 1: YELLOW (Left)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: {
                    if (currentAspect === "DOUBLE_YELLOW") return yellowAspectColor
                    return lampOffColor
                }
                border.color: borderColor
                border.width: borderWidth
            }

            // **LAMP 2: RED (Center-Left)**
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

            // **LAMP 3: YELLOW (Center-Right)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: {
                    if (currentAspect === "SINGLE_YELLOW" || currentAspect === "DOUBLE_YELLOW") {
                        return yellowAspectColor
                    }
                    return lampOffColor
                }
                border.color: borderColor
                border.width: borderWidth
            }

            // **LAMP 4: GREEN (Right)**
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
        layoutDirection: Qt.RightToLeft  // This reverses the visual order
        spacing: 0

        // **MAST GROUP** (appears on right due to RightToLeft)
        Rectangle {
            id: downMast
            width: mastWidth
            height: mastHeight
            color: mastColor
        }

        // **ARM GROUP** (appears in middle due to RightToLeft)
        Rectangle {
            id: downArm
            width: armWidth
            height: armHeight
            color: armColor
            anchors.verticalCenter: parent.verticalCenter
        }

        // **SIGNAL CIRCLES** (appears on left due to RightToLeft)
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: circleSpacing

            // **LAMP 1: YELLOW (Left)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: {
                    if (currentAspect === "DOUBLE_YELLOW") return yellowAspectColor
                    return lampOffColor
                }
                border.color: borderColor
                border.width: borderWidth
            }

            // **LAMP 2: RED (Center-Left)**
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

            // **LAMP 3: YELLOW (Center-Right)**
            Rectangle {
                width: circleWidth
                height: circleHeight
                radius: width / 2
                color: {
                    if (currentAspect === "SINGLE_YELLOW" || currentAspect === "DOUBLE_YELLOW") {
                        return yellowAspectColor
                    }
                    return lampOffColor
                }
                border.color: borderColor
                border.width: borderWidth
            }

            // **LAMP 4: GREEN (Right)**
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
            console.log("Outer signal clicked:", signalId, "Current aspect:", currentAspect, "Direction:", direction)
            outerSignal.signalClicked(signalId, currentAspect)
        }
    }
}
