import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: routeAssignmentDialog

    // === PROPERTIES ===
    property string sourceSignalId: ""
    property string sourceSignalName: ""
    property var availableSignals: []
    property string selectedDestSignalId: ""
    property bool isVisible: false

    // === DIALOG CONFIGURATION ===
    width: 280
    height: 280  // Increased height
    visible: isVisible
    z: 1000  // High z-order to appear on top

    // === MODERN COLOR PALETTE ===
    readonly property color cardBackground: "#ffffff"
    readonly property color textPrimary: "#334155"
    readonly property color textSecondary: "#64748b"
    readonly property color textMuted: "#94a3b8"
    readonly property color accentBlue: "#0ea5e9"
    readonly property color accentBlueDark: "#0284c7"
    readonly property color successGreen: "#10b981"
    readonly property color errorRed: "#ef4444"
    readonly property color borderLight: "#e2e8f0"
    readonly property color borderMedium: "#cbd5e1"
    readonly property color hoverBackground: "#f1f5f9"

    // === DRAGGABLE FUNCTIONALITY ===
    property point dragOffset: Qt.point(0, 0)
    property bool isDragging: false

    // Position in center initially
    Component.onCompleted: {
        if (parent) {
            x = (parent.width - width) / 2
            y = (parent.height - height) / 2
        }
    }

    // Native shadow layers
    Rectangle {
        anchors.fill: parent
        anchors.margins: -6
        color: "#000000"
        opacity: 0.04
        radius: 16
        z: -3
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        color: "#000000"
        opacity: 0.04
        radius: 15
        z: -2
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        color: "#000000"
        opacity: 0.04
        radius: 14
        z: -1
    }

    // Main dialog background
    color: cardBackground
    radius: 14
    border.color: borderLight
    border.width: 1

    // Subtle gradient overlay
    Rectangle {
        anchors.fill: parent
        radius: 14
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#ffffff" }
            GradientStop { position: 1.0; color: "#f8fafc" }
        }
        opacity: 0.7
    }

    // === DRAG AREA (Top portion for dragging) ===
    MouseArea {
        id: dragArea
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 40
        cursorShape: Qt.SizeAllCursor

        onPressed: {
            isDragging = true
            dragOffset = Qt.point(mouse.x, mouse.y)
        }

        onPositionChanged: {
            if (isDragging && pressed) {
                var newX = routeAssignmentDialog.x + mouse.x - dragOffset.x
                var newY = routeAssignmentDialog.y + mouse.y - dragOffset.y

                // Keep dialog within parent bounds
                if (routeAssignmentDialog.parent) {
                    newX = Math.max(0, Math.min(newX, routeAssignmentDialog.parent.width - routeAssignmentDialog.width))
                    newY = Math.max(0, Math.min(newY, routeAssignmentDialog.parent.height - routeAssignmentDialog.height))
                }

                routeAssignmentDialog.x = newX
                routeAssignmentDialog.y = newY
            }
        }

        onReleased: {
            isDragging = false
        }

        // Visual drag indicator
        Rectangle {
            anchors.centerIn: parent
            width: 30
            height: 4
            radius: 2
            color: borderMedium
            opacity: parent.containsMouse ? 0.8 : 0.4

            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
    }

    // === CLOSE BUTTON ===
    MouseArea {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        width: 24
        height: 24
        cursorShape: Qt.PointingHandCursor

        onClicked: close()

        Rectangle {
            anchors.fill: parent
            radius: 12
            color: parent.containsMouse ? "#f1f5f9" : "transparent"

            Text {
                anchors.centerIn: parent
                text: "×"
                font.pixelSize: 16
                font.weight: Font.Bold
                color: textSecondary
            }
        }
    }

    // === ANIMATIONS ===
    PropertyAnimation {
        id: showAnimation
        target: routeAssignmentDialog
        property: "scale"
        from: 0.9
        to: 1.0
        duration: 200
        easing.type: Easing.OutCubic
    }

    PropertyAnimation {
        id: fadeInAnimation
        target: routeAssignmentDialog
        property: "opacity"
        from: 0.0
        to: 1.0
        duration: 200
        easing.type: Easing.OutCubic
    }

    // === MAIN CONTENT ===
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        anchors.topMargin: 50  // Space for drag area
        anchors.bottomMargin: 20
        spacing: 16

        // === FROM SIGNAL (READ-ONLY) ===
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: cardBackground
            border.color: borderLight
            border.width: 1
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: "From:"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: textSecondary
                    Layout.minimumWidth: 35
                }

                Text {
                    text: sourceSignalId || "No signal"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: sourceSignalId ? textPrimary : textMuted
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: sourceSignalId ? successGreen : errorRed

                    SequentialAnimation on opacity {
                        running: sourceSignalId
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.5; duration: 1000 }
                        NumberAnimation { to: 1.0; duration: 1000 }
                    }
                }
            }
        }

        // === TO SIGNAL (DROPDOWN) ===
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "To:"
                font.pixelSize: 12
                font.weight: Font.Medium
                color: textSecondary
            }

            ComboBox {
                id: destSignalCombo
                Layout.fillWidth: true
                Layout.preferredHeight: 44

                model: availableSignals
                textRole: "display"
                valueRole: "signalId"

                displayText: currentIndex >= 0 ? currentText : "Select destination..."

                onCurrentValueChanged: {
                    selectedDestSignalId = currentValue || ""
                }

                background: Rectangle {
                    color: destSignalCombo.hovered ? hoverBackground : cardBackground
                    border.color: destSignalCombo.activeFocus ? accentBlue : borderMedium
                    border.width: destSignalCombo.activeFocus ? 2 : 1
                    radius: 8

                    Behavior on border.color { ColorAnimation { duration: 200 } }
                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                contentItem: Text {
                    text: destSignalCombo.displayText
                    font.pixelSize: 13
                    color: destSignalCombo.currentIndex >= 0 ? textPrimary : textMuted
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 12
                }

                popup: Popup {
                    y: destSignalCombo.height + 2
                    width: destSignalCombo.width
                    height: Math.min(contentItem.implicitHeight + 12, 200)

                    background: Rectangle {
                        color: cardBackground
                        border.color: borderLight
                        border.width: 1
                        radius: 8

                        // Native popup shadow
                        Rectangle {
                            anchors.fill: parent
                            anchors.topMargin: 2
                            anchors.leftMargin: 1
                            anchors.rightMargin: -1
                            anchors.bottomMargin: -1
                            color: "#000000"
                            opacity: 0.05
                            radius: 8
                            z: -1
                        }
                    }

                    contentItem: ListView {
                        anchors.margins: 6
                        implicitHeight: contentHeight
                        model: destSignalCombo.popup.visible ? destSignalCombo.delegateModel : null
                        currentIndex: destSignalCombo.highlightedIndex

                        ScrollIndicator.vertical: ScrollIndicator {
                            active: true
                        }
                    }
                }

                delegate: ItemDelegate {
                    width: destSignalCombo.width - 12
                    height: 36

                    background: Rectangle {
                        color: parent.hovered ? hoverBackground : "transparent"
                        radius: 4
                    }

                    contentItem: Text {
                        text: model.display
                        font.pixelSize: 13
                        color: textPrimary
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }
                }
            }
        }

        // === SPACER ===
        Item {
            Layout.fillHeight: true
            Layout.minimumHeight: 10
        }

        // === ACTION BUTTONS (FIXED TO STAY WITHIN BOUNDS) ===
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            spacing: 8

            // Request Route Button (LEFT)
            Button {
                text: "Request"
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                enabled: sourceSignalId && selectedDestSignalId

                onClicked: submitRouteRequest()

                background: Rectangle {
                    color: {
                        if (!parent.enabled) return borderLight
                        if (parent.pressed) return accentBlueDark
                        if (parent.hovered) return accentBlue
                        return accentBlue
                    }
                    radius: 6

                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: parent.enabled ? "#ffffff" : textMuted
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            // Cancel Button (RIGHT)
            Button {
                text: "Cancel"
                Layout.fillWidth: true
                Layout.preferredHeight: 40

                onClicked: close()

                background: Rectangle {
                    color: {
                        if (parent.pressed) return borderMedium
                        if (parent.hovered) return hoverBackground
                        return "transparent"
                    }
                    border.color: borderMedium
                    border.width: 1
                    radius: 6

                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // === FUNCTIONS ===
    function openForSignal(signalId, signalName) {
        sourceSignalId = signalId
        sourceSignalName = signalName || signalId
        loadAvailableSignals()
        resetForm()
        open()
    }

    function open() {
        isVisible = true
        showAnimation.start()
        fadeInAnimation.start()
    }

    function close() {
        isVisible = false
    }

    function loadAvailableSignals() {
        if (!globalDatabaseManager) {
            console.error("❌ DatabaseManager not available for loading signals")
            return
        }

        var signals = globalDatabaseManager.getAllSignalsList()
        var signalOptions = []

        for (var i = 0; i < signals.length; i++) {
            var signal = signals[i]
            if (signal.signal_id !== sourceSignalId) {
                signalOptions.push({
                    signalId: signal.signal_id,
                    display: signal.signal_id + " - " + signal.type
                })
            }
        }

        availableSignals = signalOptions
        console.log("✅ Loaded", signalOptions.length, "destination signal options")
    }

    function resetForm() {
        selectedDestSignalId = ""
        destSignalCombo.currentIndex = -1
    }

    function submitRouteRequest() {
        if (!globalRouteAssignmentService) {
            console.error("❌ RouteAssignmentService not available")
            return
        }

        console.log("🎯 Submitting minimal route request:")
        console.log("   From:", sourceSignalId)
        console.log("   To:", selectedDestSignalId)

        // Submit with default values for removed fields
        var routeId = globalRouteAssignmentService.requestRoute(
            sourceSignalId,
            selectedDestSignalId,
            "UP",        // Default direction
            "operator",  // Default operator
            {},          // Empty train data
            "NORMAL"     // Default priority
        )

        if (routeId && routeId.length > 0) {
            console.log("✅ Route request submitted successfully. Route ID:", routeId)
            close()
        } else {
            console.error("❌ Route request failed")
        }
    }
}
