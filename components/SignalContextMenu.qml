// components/SignalContextMenu.qml
import QtQuick
import QtQuick.Controls

Item {
    id: contextMenu
    anchors.fill: parent
    visible: false
    z: 999

    property string signalId: ""
    property string signalName: ""
    property string currentAspect: ""
    property var possibleAspects: []
    property real menuX: 0
    property real menuY: 0

    signal aspectSelected(string signalId, string selectedAspect)
    signal closeRequested()

    function show(x, y, sigId, sigName, current, possible) {
        signalId = sigId
        signalName = sigName
        currentAspect = current
        possibleAspects = possible
        menuX = Math.min(x, parent.width - menuContainer.width - 10)
        menuY = Math.min(y, parent.height - menuContainer.height - 10)
        visible = true
        showAnimation.start()
    }

    function hide() {
        hideAnimation.start()
    }

    // Background overlay
    Rectangle {
        anchors.fill: parent
        color: "transparent"

        MouseArea {
            anchors.fill: parent
            onClicked: contextMenu.hide()
        }
    }

    // Menu container
    Rectangle {
        id: menuContainer
        x: menuX
        y: menuY
        width: Math.max(220, contentColumn.width + 24)
        height: contentColumn.height + 24
        color: "#2d3748"
        border.color: "#4a5568"
        border.width: 1
        radius: 6

        // Drop shadow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            color: "#40000000"
            radius: parent.radius + 1
            z: -1
        }

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 2

            // Header
            Rectangle {
                width: parent.width
                height: headerText.contentHeight + 8
                color: "#1a1a1a"
                radius: 4

                Text {
                    id: headerText
                    anchors.centerIn: parent
                    text: signalName + " (" + signalId + ")"
                    font.pixelSize: 12
                    font.weight: Font.Bold
                    color: "#ffffff"
                }
            }

            // Current aspect
            Rectangle {
                width: parent.width
                height: 24
                color: "#374151"
                radius: 3

                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 8
                    spacing: 8

                    Rectangle {
                        width: 12
                        height: 12
                        radius: 6
                        color: getAspectColor(currentAspect)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: "Current: " + currentAspect
                        font.pixelSize: 10
                        color: "#a0aec0"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // Separator
            Rectangle {
                width: parent.width
                height: 1
                color: "#4a5568"
            }

            // Possible aspects
            Text {
                text: "Change to:"
                font.pixelSize: 10
                font.weight: Font.Bold
                color: "#a0aec0"
                leftPadding: 4
            }

            Repeater {
                model: possibleAspects  // ✅ Use the actual data now that we know it works

                Rectangle {
                    id: aspectItem  // ✅ Give the Rectangle an ID for easier access
                    width: parent.width
                    height: 28
                    color: aspectMouseArea.containsMouse ? "#3182ce" : "transparent"
                    radius: 3

                    property string aspectName: modelData  // ✅ This works fine
                    property bool isCurrent: aspectName === currentAspect

                    Row {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 8
                        spacing: 8

                        Rectangle {
                            width: 14
                            height: 14
                            radius: 7
                            color: getAspectColor(aspectItem.aspectName)  // ✅ FIXED: Use aspectItem.aspectName
                            border.color: aspectItem.isCurrent ? "#ffffff" : "transparent"  // ✅ FIXED
                            border.width: 2
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: aspectItem.aspectName + (aspectItem.isCurrent ? " (current)" : "")  // ✅ FIXED
                            font.pixelSize: 12
                            color: aspectItem.isCurrent ? "#a0aec0" : "#ffffff"  // ✅ FIXED
                            font.weight: aspectItem.isCurrent ? Font.Normal : Font.Bold  // ✅ FIXED
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: aspectItem.isCurrent ? "" : "→"  // ✅ FIXED
                            font.pixelSize: 14
                            color: "#3182ce"
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !aspectItem.isCurrent  // ✅ FIXED
                        }
                    }

                    MouseArea {
                        id: aspectMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: aspectItem.isCurrent ? Qt.ArrowCursor : Qt.PointingHandCursor  // ✅ FIXED
                        enabled: !aspectItem.isCurrent  // ✅ FIXED

                        onClicked: {
                            if (!aspectItem.isCurrent) {  // ✅ FIXED
                                console.log("Aspect selected:", aspectItem.aspectName)
                                contextMenu.aspectSelected(signalId, aspectItem.aspectName)  // ✅ FIXED
                                contextMenu.hide()
                            }
                        }
                    }
                }
            }
        }
    }

    // Animations
    NumberAnimation {
        id: showAnimation
        target: menuContainer
        property: "scale"
        from: 0.9
        to: 1.0
        duration: 150
        easing.type: Easing.OutQuad
    }

    SequentialAnimation {
        id: hideAnimation
        NumberAnimation {
            target: menuContainer
            property: "scale"
            from: 1.0
            to: 0.9
            duration: 100
            easing.type: Easing.InQuad
        }
        ScriptAction {
            script: {
                contextMenu.visible = false
                contextMenu.closeRequested()
            }
        }
    }

    // Helper function for aspect colors
    function getAspectColor(aspect) {
        switch(aspect) {
            case "RED": return "#ff0000"
            case "YELLOW": return "#ffff00"
            case "SINGLE_YELLOW": return "#ffff00"
            case "DOUBLE_YELLOW": return "#f6ad55"
            case "GREEN": return "#00ff00"
            case "WHITE": return "#ffffff"
            case "BLUE": return "#3182ce"
            default: return "#404040"
        }
    }
}
