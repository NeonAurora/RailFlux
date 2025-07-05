// StationData.js - QML Compatible Version
.pragma library

// Core station layout data - ported from your firebase_test.py
var trackSegments = [
    // T1 Track segments
    { id: "T1S1",  startRow: 60, startCol: 0, endRow: 60, endCol: 13, occupied: false },
    { id: "T1S2",  startRow: 60, startCol: 13, endRow: 60, endCol: 35, occupied: false },
    { id: "T1S3",  startRow: 60, startCol: 35, endRow: 60, endCol: 68, occupied: false },
    { id: "T1S4",  startRow: 60, startCol: 68, endRow: 60, endCol: 94, occupied: false },
    { id: "T1S5",  startRow: 60, startCol: 97, endRow: 60, endCol: 154, occupied: false },
    { id: "T1S6",  startRow: 60, startCol: 154, endRow: 60, endCol: 233, occupied: false },
    { id: "T1S7",  startRow: 60, startCol: 233, endRow: 60, endCol: 279, occupied: false },
    { id: "T1S8",  startRow: 60, startCol: 279, endRow: 60, endCol: 306, occupied: false },
    { id: "T1S9",  startRow: 60, startCol: 306, endRow: 60, endCol: 339, occupied: false },
    { id: "T1S10", startRow: 60, startCol: 339, endRow: 60, endCol: 359, occupied: false },
    { id: "T1S11", startRow: 60, startCol: 359, endRow: 60, endCol: 370, occupied: false },

    // T4 Track segments
    { id: "T4S1", startRow: 40, startCol: 114, endRow: 40, endCol: 155, occupied: false },
    { id: "T4S2", startRow: 40, startCol: 155, endRow: 40, endCol: 165, occupied: false },
    { id: "T4S3", startRow: 40, startCol: 165, endRow: 40, endCol: 175, occupied: false },
    { id: "T4S4", startRow: 40, startCol: 175, endRow: 40, endCol: 259, occupied: false },

    // Connection tracks
    { id: "T5S1", startRow: 57, startCol: 97, endRow: 40, endCol: 114, occupied: true },
    { id: "T6S1", startRow: 40, startCol: 259, endRow: 60, endCol: 279, occupied: false },
];

// Basic signal positions
var signalPositions = [
    { id: "SG001", type: "BASIC", row: 57, col: 4, name: "T1S1", aspect: "RED" },
    { id: "SG002", type: "BASIC", row: 57, col: 19, name: "T1S2", aspect: "RED" },
    { id: "SG003", type: "BASIC", row: 57, col: 39, name: "T1S3", aspect: "RED" },
    { id: "SG004", type: "BASIC", row: 67, col: 26, name: "T2S1", aspect: "RED" },
    { id: "SG005", type: "BASIC", row: 37, col: 105, name: "T4S1", aspect: "RED" }
];

// Text labels from your text_config.txt
var textLabels = [
    // Grid reference marks
    { text: "50", row: 1, col: 49, fontSize: 12 },
    { text: "100", row: 1, col: 99, fontSize: 12 },
    { text: "150", row: 1, col: 149, fontSize: 12 },
    { text: "200", row: 1, col: 199, fontSize: 12 },
    { text: "30", row: 29, col: 1, fontSize: 12 },
    { text: "90", row: 89, col: 1, fontSize: 12 },

    // Track segment labels
    { text: "T1S1", row: 57, col: 7, fontSize: 12 },
    { text: "T1S2", row: 57, col: 24, fontSize: 12 },
    { text: "T1S3", row: 57, col: 51, fontSize: 12 },
    { text: "T1S4", row: 57, col: 81, fontSize: 12 },
    { text: "T1S5", row: 57, col: 124, fontSize: 12 },
    { text: "T1S6", row: 57, col: 193, fontSize: 12 },
    { text: "T1S7", row: 57, col: 256, fontSize: 12 },
    { text: "T1S8", row: 57, col: 290, fontSize: 12 },
    { text: "T1S9", row: 57, col: 318, fontSize: 12 },
    { text: "T1S10", row: 57, col: 346, fontSize: 12 },
    { text: "T1S11", row: 57, col: 360, fontSize: 12 },
    { text: "T4S1", row: 37, col: 105, fontSize: 12 },

    // Level crossing labels
    { text: "LC_GATE1", row: 37, col: 36, fontSize: 12 },
    { text: "LC_GATE2", row: 37, col: 211, fontSize: 12 }
];

// Level crossings
var levelCrossings = [
    { id: "LC001", row: 37, col: 36, state: "OPEN", name: "LC_GATE1" },
    { id: "LC002", row: 37, col: 211, state: "OPEN", name: "LC_GATE2" }
];

// Utility functions (port of your canvas.py logic)
function gridToPixel(row, col, cellSize) {
    return {
        x: col * cellSize,
        y: row * cellSize
    };
}

function pixelToGrid(x, y, cellSize) {
    return {
        row: Math.floor(y / cellSize),
        col: Math.floor(x / cellSize)
    };
}
