import QtQuick

Item {
    id: basicSignal

    property string signalId: ""
    property string signalName: ""
    property string currentAspect: "RED"
    property int cellSize: 20

    // Simple signal representation - we'll enhance this later
    width: cellSize
    height: cellSize

    signal signalClicked(string signalId)

    Rectangle {
        id: signalHead
        width: parent.width * 0.8
        height: parent.height * 0.8
        anchors.centerIn: parent
        color: getAspectColor()
        border.color: "#ffffff"
        border.width: 1
        radius: width / 2

        // Hover effect
        Rectangle {
            anchors.fill: parent
            color: "white"
            opacity: mouseArea.containsMouse ? 0.3 : 0
            radius: parent.radius

            Behavior on opacity {
                NumberAnimation { duration: 150 }
            }
        }
    }

    // Signal ID label
    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: -12
        text: signalId
        color: "#ffffff"
        font.pixelSize: 8
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            console.log("Signal clicked:", signalId, currentAspect)
            basicSignal.signalClicked(signalId)
        }
    }

    function getAspectColor() {
        switch(currentAspect) {
            case "RED": return "#e53e3e"
            case "YELLOW": return "#d69e2e"
            case "GREEN": return "#38a169"
            default: return "#e53e3e"
        }
    }
}
