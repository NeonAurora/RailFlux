// components/ToastNotification.qml
import QtQuick
import QtQuick.Controls

Item {
    id: toast
    anchors.fill: parent
    visible: false
    z: 1000  // High z-order to appear above everything

    property string title: ""
    property string message: ""
    property string signalId: ""
    property string blockingReason: ""
    property int duration: 5000  // 5 seconds

    signal closeRequested()

    function show(titleText, messageText, signalIdText, reasonText) {
        title = titleText
        message = messageText
        signalId = signalIdText
        blockingReason = reasonText
        visible = true
        autoHideTimer.restart()
        showAnimation.start()
    }

    function hide() {
        hideAnimation.start()
    }

    // Semi-transparent background
    Rectangle {
        anchors.fill: parent
        color: "#80000000"
        opacity: 0.3

        MouseArea {
            anchors.fill: parent
            onClicked: toast.hide()
        }
    }

    // Toast container
    Rectangle {
        id: toastContainer
        width: Math.min(500, parent.width * 0.8)
        height: contentColumn.height + 40
        anchors.centerIn: parent
        color: "#2d3748"
        border.color: "#e53e3e"
        border.width: 2
        radius: 8

        // Drop shadow effect
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            color: "#40000000"
            radius: parent.radius + 2
            z: -1
        }

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20
            spacing: 12

            // Header with icon and title
            Row {
                width: parent.width
                spacing: 12

                Text {
                    text: "🚫"
                    font.pixelSize: 24
                    anchors.verticalCenter: parent.verticalCenter
                }

                Column {
                    width: parent.width - 60
                    spacing: 4

                    Text {
                        text: toast.title
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        color: "#e53e3e"
                        wrapMode: Text.Wrap
                        width: parent.width
                    }

                    Text {
                        text: "Signal: " + toast.signalId
                        font.pixelSize: 12
                        color: "#a0aec0"
                        width: parent.width
                    }
                }

                // Close button
                Rectangle {
                    width: 24
                    height: 24
                    color: closeMouseArea.containsMouse ? "#e53e3e" : "transparent"
                    border.color: "#ffffff"
                    border.width: 1
                    radius: 12
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        text: "×"
                        anchors.centerIn: parent
                        color: "#ffffff"
                        font.pixelSize: 16
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
                color: "#4a5568"
            }

            // Message content
            Text {
                text: toast.message
                font.pixelSize: 14
                color: "#ffffff"
                wrapMode: Text.Wrap
                width: parent.width
            }

            // Blocking reason details
            Rectangle {
                width: parent.width
                height: reasonText.contentHeight + 16
                color: "#1a1a1a"
                border.color: "#4a5568"
                border.width: 1
                radius: 4

                Text {
                    id: reasonText
                    anchors.fill: parent
                    anchors.margins: 8
                    text: "🔒 Blocking Reason: " + toast.blockingReason
                    font.pixelSize: 12
                    font.family: "monospace"
                    color: "#f6ad55"
                    wrapMode: Text.Wrap
                }
            }

            // Action buttons
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12

                Button {
                    text: "Dismiss"
                    width: 80
                    height: 32
                    onClicked: toast.hide()

                    background: Rectangle {
                        color: parent.pressed ? "#4a5568" : "#2d3748"
                        border.color: "#a0aec0"
                        border.width: 1
                        radius: 4
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 11
                    }
                }
            }
        }
    }

    // Auto-hide timer
    Timer {
        id: autoHideTimer
        interval: toast.duration
        onTriggered: toast.hide()
    }

    // Show animation
    NumberAnimation {
        id: showAnimation
        target: toastContainer
        property: "scale"
        from: 0.8
        to: 1.0
        duration: 200
        easing.type: Easing.OutBack
    }

    // Hide animation
    SequentialAnimation {
        id: hideAnimation
        NumberAnimation {
            target: toastContainer
            property: "scale"
            from: 1.0
            to: 0.8
            duration: 150
            easing.type: Easing.InBack
        }
        ScriptAction {
            script: {
                toast.visible = false
                toast.closeRequested()
            }
        }
    }
}
