// ✅ ENHANCED: components/SignalContextMenu.qml with scrolling
import QtQuick
import QtQuick.Controls

Item {
    id: contextMenu
    anchors.fill: parent
    visible: false
    z: 999

    property string signalId: ""
    property string signalName: ""
    property string signalType: ""

    // ✅ MAIN SIGNAL properties
    property string currentAspect: ""
    property var possibleAspects: []

    // ✅ SUBSIDIARY SIGNAL properties
    property string currentCallingOnAspect: ""
    property string currentLoopAspect: ""

    property real menuX: 0
    property real menuY: 0

    // ✅ LAYOUT CONSTANTS
    readonly property int maxMenuHeight: Math.min(parent.height * 0.8, 400)  // 80% of screen or 400px max
    readonly property int menuWidth: 250

    // ✅ Signal emits aspectType
    signal aspectSelected(string signalId, string aspectType, string selectedAspect)
    signal closeRequested()

    // ✅ ENHANCED: show function with better positioning
    function show(x, y, sigId, sigName, current, possible, callingOn, loop) {
        signalId = sigId
        signalName = sigName
        currentAspect = current
        possibleAspects = possible

        // Handle subsidiary signals
        if (arguments.length >= 8) {
            currentCallingOnAspect = callingOn || "OFF"
            currentLoopAspect = loop || "OFF"
            signalType = "HOME"
        } else {
            currentCallingOnAspect = "OFF"
            currentLoopAspect = "OFF"
            signalType = ""
            fetchSignalDetails()
        }

        // ✅ IMPROVED: Position menu with height consideration
        var preferredHeight = Math.min(contentColumn.implicitHeight + 24, maxMenuHeight)
        menuX = Math.min(x, parent.width - menuWidth - 10)
        menuY = Math.min(y, parent.height - preferredHeight - 10)

        visible = true
        showAnimation.start()
    }

    function fetchSignalDetails() {
        // ... (same as before)
        var dbMgr = null
        var currentParent = parent
        while (currentParent && !dbMgr) {
            if (currentParent.dbManager) {
                dbMgr = currentParent.dbManager
                break
            }
            currentParent = currentParent.parent
        }

        if (!dbMgr || !dbMgr.isConnected) return

        var signalData = dbMgr.getSignalById(signalId)
        if (signalData && Object.keys(signalData).length > 0) {
            signalType = signalData.signal_type || ""
            currentCallingOnAspect = signalData.calling_on_aspect || "OFF"
            currentLoopAspect = signalData.loop_aspect || "OFF"
        }
    }

    readonly property bool isHomeSignal: signalType === "HOME"

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

    // ✅ ENHANCED: Menu container with fixed dimensions and scrolling
    Rectangle {
        id: menuContainer
        x: menuX
        y: menuY
        width: menuWidth
        height: Math.min(contentColumn.implicitHeight + 24, maxMenuHeight)
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

        // ✅ HEADER (fixed at top)
        Rectangle {
            id: headerSection
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            height: headerText.contentHeight + 8
            color: "#1a1a1a"
            radius: 4

            Text {
                id: headerText
                anchors.centerIn: parent
                text: signalName + " (" + signalId + ")" + (isHomeSignal ? " [HOME]" : "")
                font.pixelSize: 12
                font.weight: Font.Bold
                color: "#ffffff"
            }
        }

        // ✅ SCROLLABLE CONTENT AREA
        ScrollView {
            id: scrollView
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: headerSection.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 12
            anchors.topMargin: 4  // Reduced top margin since header has its own

            clip: true

            // ✅ SCROLLBAR STYLING
            ScrollBar.vertical: ScrollBar {
                id: verticalScrollBar
                active: true
                policy: ScrollBar.AsNeeded
                size: scrollView.height / contentColumn.height

                background: Rectangle {
                    color: "#4a5568"
                    radius: 3
                }

                contentItem: Rectangle {
                    color: "#718096"
                    radius: 3
                }
            }

            // ✅ SCROLLABLE CONTENT
            Column {
                id: contentColumn
                width: scrollView.width - (verticalScrollBar.visible ? verticalScrollBar.width : 0)
                spacing: 4

                // ✅ MAIN SIGNAL SECTION
                AspectSection {
                    id: mainSignalSection
                    width: parent.width
                    sectionTitle: "Main Signal"
                    currentAspect: contextMenu.currentAspect
                    possibleAspects: contextMenu.possibleAspects
                    aspectType: "MAIN"
                    isMainSection: true

                    onAspectClicked: function(aspectType, selectedAspect) {
                        contextMenu.aspectSelected(signalId, aspectType, selectedAspect)
                        contextMenu.hide()
                    }
                }

                // ✅ SUBSIDIARY SIGNALS SECTION (only for home signals)
                Column {
                    width: parent.width
                    spacing: 4
                    visible: isHomeSignal

                    // Separator
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: "#4a5568"
                    }

                    Text {
                        text: "Subsidiary Signals"
                        font.pixelSize: 11
                        font.weight: Font.Bold
                        color: "#3182ce"
                        leftPadding: 4
                    }

                    // ✅ CALLING-ON SIGNAL SECTION
                    AspectSection {
                        width: parent.width
                        sectionTitle: "Calling-On Signal"
                        currentAspect: contextMenu.currentCallingOnAspect
                        possibleAspects: ["WHITE", "OFF"]
                        aspectType: "CALLING_ON"

                        onAspectClicked: function(aspectType, selectedAspect) {
                            contextMenu.aspectSelected(signalId, aspectType, selectedAspect)
                            contextMenu.hide()
                        }
                    }

                    // ✅ LOOP SIGNAL SECTION
                    AspectSection {
                        width: parent.width
                        sectionTitle: "Loop Signal"
                        currentAspect: contextMenu.currentLoopAspect
                        possibleAspects: ["YELLOW", "OFF"]
                        aspectType: "LOOP"

                        onAspectClicked: function(aspectType, selectedAspect) {
                            contextMenu.aspectSelected(signalId, aspectType, selectedAspect)
                            contextMenu.hide()
                        }
                    }
                }
            }
        }
    }

    // ✅ COMPACT ASPECT SECTION COMPONENT
    component AspectSection: Column {
        id: aspectSection
        spacing: 2

        property string sectionTitle: ""
        property string currentAspect: ""
        property var possibleAspects: []
        property string aspectType: ""
        property bool isMainSection: false

        signal aspectClicked(string aspectType, string selectedAspect)

        // Section title
        Text {
            text: sectionTitle
            font.pixelSize: isMainSection ? 11 : 10
            font.weight: Font.Bold
            color: isMainSection ? "#ffffff" : "#a0aec0"
            leftPadding: 4
        }

        // Current aspect display (more compact)
        Rectangle {
            width: parent.width
            height: 20
            color: "#374151"
            radius: 3

            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 6
                spacing: 4

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: getAspectColor(currentAspect)
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "Current: " + currentAspect
                    font.pixelSize: 8
                    color: "#a0aec0"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Change options (compact)
        Text {
            text: "Change to:"
            font.pixelSize: 8
            color: "#a0aec0"
            leftPadding: 4
        }

        // ✅ COMPACT: Aspect options in a more space-efficient layout
        Repeater {
            model: possibleAspects

            Rectangle {
                id: aspectItem
                width: parent.width
                height: 22  // Reduced height
                color: aspectMouseArea.containsMouse ? "#3182ce" : "transparent"
                radius: 3

                property string aspectName: modelData
                property bool isCurrent: aspectName === currentAspect

                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 6
                    spacing: 4

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: getAspectColor(aspectItem.aspectName)
                        border.color: aspectItem.isCurrent ? "#ffffff" : "transparent"
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: aspectItem.aspectName + (aspectItem.isCurrent ? " (current)" : "")
                        font.pixelSize: 10
                        color: aspectItem.isCurrent ? "#a0aec0" : "#ffffff"
                        font.weight: aspectItem.isCurrent ? Font.Normal : Font.Bold
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: aspectItem.isCurrent ? "" : "→"
                        font.pixelSize: 10
                        color: "#3182ce"
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !aspectItem.isCurrent
                    }
                }

                MouseArea {
                    id: aspectMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: aspectItem.isCurrent ? Qt.ArrowCursor : Qt.PointingHandCursor
                    enabled: !aspectItem.isCurrent

                    onClicked: {
                        if (!aspectItem.isCurrent) {
                            aspectSection.aspectClicked(aspectType, aspectItem.aspectName)
                        }
                    }
                }
            }
        }
    }

    // Animations (unchanged)
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

    function getAspectColor(aspect) {
        switch(aspect) {
            case "RED": return "#ff0000"
            case "YELLOW": return "#ffff00"
            case "SINGLE_YELLOW": return "#ffff00"
            case "DOUBLE_YELLOW": return "#f6ad55"
            case "GREEN": return "#00ff00"
            case "WHITE": return "#ffffff"
            case "BLUE": return "#3182ce"
            case "OFF": return "#404040"
            default: return "#404040"
        }
    }
}
