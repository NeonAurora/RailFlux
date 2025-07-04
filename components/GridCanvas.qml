import QtQuick

Item {
    id: canvas
    
    property int gridSize: 20
    property bool showGrid: true
    property color gridColor: "#333333"
    property real gridOpacity: 0.3
    
    // Vertical grid lines
    Repeater {
        model: showGrid ? Math.ceil(canvas.width / gridSize) : 0
        Rectangle {
            x: index * gridSize
            y: 0
            width: 1
            height: canvas.height
            color: canvas.gridColor
            opacity: canvas.gridOpacity
        }
    }
    
    // Horizontal grid lines
    Repeater {
        model: showGrid ? Math.ceil(canvas.height / gridSize) : 0
        Rectangle {
            x: 0
            y: index * gridSize
            width: canvas.width
            height: 1
            color: canvas.gridColor
            opacity: canvas.gridOpacity
        }
    }
}