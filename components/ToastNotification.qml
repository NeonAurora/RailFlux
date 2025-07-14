// components/ToastNotification.qml
import QtQuick
import QtQuick.Controls

Item {
    id: toast
    anchors.fill: parent
    visible: false
    z: 1000  // High z-order to appear above everything

    // ✅ REFACTORED: Generic properties
    property string title: ""
    property string message: ""
    property string entityId: ""           // Generic entity (signal, track, etc.)
    property string entityType: "SIGNAL"   // SIGNAL, TRACK, SYSTEM, etc.
    property string details: ""
    property string toastType: "WARNING"   // WARNING, ERROR, CRITICAL, INFO
    property int duration: 5000            // 5 seconds (ignored for CRITICAL)
    property bool autoHide: true           // Can be disabled for critical alerts

    signal closeRequested()

    // ✅ ENHANCED: Generic show function with flexible parameters
    function show(titleText, messageText, entityIdText, detailsText, type, autoHideEnabled) {
        title = titleText || "Notification"
        message = messageText || ""
        entityId = entityIdText || ""
        details = detailsText || ""
        toastType = type || "WARNING"
        autoHide = autoHideEnabled !== undefined ? autoHideEnabled : true

        // ✅ Set entity type based on entity ID pattern
        if (entityId.startsWith("T")) {
            entityType = "TRACK"
        } else if (entityId.startsWith("ST") || entityId.startsWith("HM") || entityId.startsWith("OT") || entityId.startsWith("AS")) {
            entityType = "SIGNAL"
        } else if (entityId.startsWith("PM")) {
            entityType = "POINT_MACHINE"
        } else {
            entityType = "SYSTEM"
        }

        visible = true
        if (autoHide) {
            autoHideTimer.restart()
        }
        showAnimation.start()
    }

    // ✅ LEGACY SUPPORT: Keep old function signature for backward compatibility
    function showSignalBlocked(titleText, messageText, signalIdText, reasonText) {
        show(titleText, messageText, signalIdText, reasonText, "ERROR", true)
    }

    function hide() {
        hideAnimation.start()
    }

    // ✅ UPDATED: Frosted glass background effect
    Rectangle {
        anchors.fill: parent
        color: "#ffffff"
        opacity: 0.15  // Light white overlay for glass effect

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (autoHide) {
                    toast.hide()
                }
            }
        }
    }

    // ✅ ENHANCED: Toast container with glass effect
    Rectangle {
        id: toastContainer
        width: Math.min(600, parent.width * 0.85)
        height: contentColumn.height + 40
        anchors.centerIn: parent

        // ✅ Glass effect background
        color: "#ffffff"
        opacity: 0.95
        border.color: getToastBorderColor()
        border.width: 2
        radius: 12

        // ✅ Enhanced drop shadow with blur effect
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            color: "#40000000"
            radius: parent.radius + 4
            z: -1
            opacity: 0.3
        }

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 16

            // ✅ ENHANCED: Header with dynamic icon and styling
            Row {
                width: parent.width
                spacing: 16

                Text {
                    text: getToastIcon()
                    font.pixelSize: getIconSize()
                    anchors.verticalCenter: parent.verticalCenter
                    color: getToastColor()
                }

                Column {
                    width: parent.width - 80
                    spacing: 6

                    Text {
                        text: toast.title
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: "#1a1a1a"  // Dark text on white background
                        wrapMode: Text.Wrap
                        width: parent.width
                    }

                    Text {
                        text: getEntityLabel()
                        font.pixelSize: 13
                        color: "#4a5568"
                        width: parent.width
                        visible: entityId !== ""
                    }
                }

                // ✅ CONDITIONAL: Close button (always visible for manual dismiss)
                Rectangle {
                    width: 28
                    height: 28
                    color: closeMouseArea.containsMouse ? getToastColor() : "transparent"
                    border.color: "#4a5568"
                    border.width: 1
                    radius: 14
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        text: "×"
                        anchors.centerIn: parent
                        color: closeMouseArea.containsMouse ? "#ffffff" : "#4a5568"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        id: closeMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: toast.hide()
                    }
                }
            }

            // Separator
            Rectangle {
                width: parent.width
                height: 1
                color: "#e2e8f0"
            }

            // ✅ ENHANCED: Message content with better styling
            Text {
                text: toast.message
                font.pixelSize: 15
                color: "#2d3748"
                wrapMode: Text.Wrap
                width: parent.width
                lineHeight: 1.4
            }

            // ✅ ENHANCED: Details section (replaces blocking reason)
            Rectangle {
                width: parent.width
                height: detailsText.contentHeight + 20
                color: "#f7fafc"
                border.color: "#e2e8f0"
                border.width: 1
                radius: 6
                visible: details !== ""

                Text {
                    id: detailsText
                    anchors.fill: parent
                    anchors.margins: 12
                    text: "🔍 " + toast.details
                    font.pixelSize: 12
                    font.family: "monospace"
                    color: "#4a5568"
                    wrapMode: Text.Wrap
                }
            }

            // ✅ ENHANCED: Action buttons with dynamic styling
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 16

                // ✅ CRITICAL: Always show dismiss for non-auto-hide toasts
                Button {
                    text: toastType === "CRITICAL" ? "ACKNOWLEDGE" : "Dismiss"
                    width: toastType === "CRITICAL" ? 140 : 100
                    height: 36
                    onClicked: toast.hide()

                    background: Rectangle {
                        color: parent.pressed ? getToastColorDark() : getToastColor()
                        border.color: getToastColor()
                        border.width: 1
                        radius: 6
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "#ffffff"
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 12
                    }
                }

                // ✅ CRITICAL: Emergency contact button for system freeze
                Button {
                    text: "🚨 EMERGENCY"
                    width: 120
                    height: 36
                    visible: toastType === "CRITICAL"
                    onClicked: {
                        console.log("🚨 Emergency contact requested - implement emergency procedures")
                        // TODO: Future implementation - emergency contact system
                    }

                    background: Rectangle {
                        color: parent.pressed ? "#dc2626" : "#ef4444"
                        border.color: "#dc2626"
                        border.width: 1
                        radius: 6
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "#ffffff"
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 11
                    }
                }
            }
        }
    }

    // ✅ CONDITIONAL: Auto-hide timer (disabled for critical alerts)
    Timer {
        id: autoHideTimer
        interval: toast.duration
        onTriggered: {
            if (autoHide) {
                toast.hide()
            }
        }
    }

    // Enhanced animations
    NumberAnimation {
        id: showAnimation
        target: toastContainer
        property: "scale"
        from: 0.85
        to: 1.0
        duration: 250
        easing.type: Easing.OutBack
    }

    SequentialAnimation {
        id: hideAnimation
        NumberAnimation {
            target: toastContainer
            property: "scale"
            from: 1.0
            to: 0.85
            duration: 200
            easing.type: Easing.InBack
        }
        ScriptAction {
            script: {
                toast.visible = false
                toast.closeRequested()
            }
        }
    }

    // ✅ HELPER FUNCTIONS: Dynamic styling based on toast type
    function getToastIcon() {
        switch(toastType) {
            case "CRITICAL": return "🚨"
            case "ERROR": return "🚫"
            case "WARNING": return "⚠️"
            case "INFO": return "ℹ️"
            default: return "🔔"
        }
    }

    function getIconSize() {
        return toastType === "CRITICAL" ? 32 : 24
    }

    function getToastColor() {
        switch(toastType) {
            case "CRITICAL": return "#dc2626"  // Red
            case "ERROR": return "#e53e3e"     // Red
            case "WARNING": return "#f59e0b"   // Orange
            case "INFO": return "#3182ce"      // Blue
            default: return "#6b7280"          // Gray
        }
    }

    function getToastColorDark() {
        switch(toastType) {
            case "CRITICAL": return "#b91c1c"
            case "ERROR": return "#dc2626"
            case "WARNING": return "#d97706"
            case "INFO": return "#2563eb"
            default: return "#4b5563"
        }
    }

    function getToastBorderColor() {
        return getToastColor()
    }

    function getEntityLabel() {
        if (entityId === "") return ""

        switch(entityType) {
            case "SIGNAL": return "Signal: " + entityId
            case "TRACK": return "Track: " + entityId
            case "POINT_MACHINE": return "Point Machine: " + entityId
            case "SYSTEM": return "System Component: " + entityId
            default: return "Entity: " + entityId
        }
    }
}
