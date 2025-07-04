// Core station layout data - ported from your firebase_test.py
const trackSegments = [
    // T1 Track segments
    { id: "T1S1", startRow: 60, startCol: 0, endRow: 60, endCol: 10, occupied: false },
    { id: "T1S2", startRow: 60, startCol: 10, endRow: 60, endCol: 30, occupied: false },
    { id: "T1S3", startRow: 60, startCol: 30, endRow: 60, endCol: 50, occupied: false },
    { id: "T1S4", startRow: 60, startCol: 50, endRow: 60, endCol: 100, occupied: false },
    { id: "T1S5", startRow: 60, startCol: 100, endRow: 60, endCol: 120, occupied: false },
    { id: "T1S6", startRow: 60, startCol: 120, endRow: 60, endCol: 160, occupied: false },
    { id: "T1S7", startRow: 60, startCol: 160, endRow: 60, endCol: 180, occupied: false },
    { id: "T1S8", startRow: 60, startCol: 180, endRow: 60, endCol: 200, occupied: false },
    { id: "T1S9", startRow: 60, startCol: 200, endRow: 60, endCol: 250, occupied: false },

    // T2 Track segments
    { id: "T2S1", startRow: 70, startCol: 0, endRow: 70, endCol: 53, occupied: false },
    { id: "T2S2", startRow: 70, startCol: 53, endRow: 70, endCol: 70, occupied: false },
    { id: "T2S3", startRow: 70, startCol: 70, endRow: 70, endCol: 90, occupied: false },
    { id: "T2S4", startRow: 70, startCol: 90, endRow: 70, endCol: 119, occupied: false },
    { id: "T2S5", startRow: 70, startCol: 119, endRow: 70, endCol: 163, occupied: false },
    { id: "T2S6", startRow: 70, startCol: 163, endRow: 70, endCol: 182, occupied: false },
    { id: "T2S7", startRow: 70, startCol: 182, endRow: 70, endCol: 200, occupied: false },
    { id: "T2S8", startRow: 70, startCol: 200, endRow: 70, endCol: 250, occupied: false },

    // Junction tracks (your T3)
    { id: "T3S1", startRow: 70, startCol: 95, endRow: 65, endCol: 100, occupied: false },
    { id: "T3S2", startRow: 65, startCol: 100, endRow: 60, endCol: 105, occupied: false },

    // T4 Track segments
    { id: "T4S1", startRow: 40, startCol: 100, endRow: 40, endCol: 110, occupied: false },
    { id: "T4S2", startRow: 40, startCol: 110, endRow: 40, endCol: 135, occupied: false },
    { id: "T4S3", startRow: 40, startCol: 135, endRow: 40, endCol: 150, occupied: false },
    { id: "T4S4", startRow: 40, startCol: 150, endRow: 40, endCol: 180, occupied: false },

    // Connection tracks
    { id: "T5S1", startRow: 60, startCol: 110, endRow: 40, endCol: 130, occupied: false },
    { id: "T6S1", startRow: 40, startCol: 170, endRow: 60, endCol: 190, occupied: false },

    // T7 Track segments
    { id: "T7S1", startRow: 90, startCol: 110, endRow: 90, endCol: 130, occupied: false },
    { id: "T7S2", startRow: 90, startCol: 130, endRow: 90, endCol: 140, occupied: false },
    { id: "T7S3", startRow: 90, startCol: 140, endRow: 90, endCol: 180, occupied: false },

    // More connection tracks
    { id: "T8S1", startRow: 70, startCol: 100, endRow: 90, endCol: 120, occupied: false },
    { id: "T9S1", startRow: 90, startCol: 170, endRow: 70, endCol: 190, occupied: false }
];

// Basic signal positions (we'll expand this later with proper signal logic)
const signalPositions = [
    { id: "SG001", type: "BASIC", row: 57, col: 4, name: "T1S1", aspect: "RED" },
    { id: "SG002", type: "BASIC", row: 57, col: 19, name: "T1S2", aspect: "RED" },
    { id: "SG003", type: "BASIC", row: 57, col: 39, name: "T1S3", aspect: "RED" },
    { id: "SG004", type: "BASIC", row: 67, col: 26, name: "T2S1", aspect: "RED" },
    { id: "SG005", type: "BASIC", row: 37, col: 105, name: "T4S1", aspect: "RED" }
];

// Text labels from your text_config.txt
const textLabels = [
    // Grid reference marks
    { text: "50", row: 1, col: 49, fontSize: 12 },
    { text: "100", row: 1, col: 99, fontSize: 12 },
    { text: "150", row: 1, col: 149, fontSize: 12 },
    { text: "200", row: 1, col: 199, fontSize: 12 },
    { text: "30", row: 29, col: 1, fontSize: 12 },
    { text: "90", row: 89, col: 1, fontSize: 12 },

    // Track segment labels
    { text: "T1S1", row: 57, col: 4, fontSize: 12 },
    { text: "T1S2", row: 57, col: 19, fontSize: 12 },
    { text: "T1S3", row: 57, col: 39, fontSize: 12 },
    { text: "T1S4", row: 57, col: 74, fontSize: 12 },
    { text: "T2S1", row: 67, col: 26, fontSize: 12 },
    { text: "T2S2", row: 67, col: 61, fontSize: 12 },
    { text: "T4S1", row: 37, col: 105, fontSize: 12 },
    { text: "T7S1", row: 92, col: 120, fontSize: 12 },

    // Level crossing labels
    { text: "LC_GATE1", row: 37, col: 36, fontSize: 12 },
    { text: "LC_GATE2", row: 37, col: 211, fontSize: 12 }
];

// Level crossings from your setup
const levelCrossings = [
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

// Export core data only
export { trackSegments, signalPositions, textLabels, levelCrossings, gridToPixel, pixelToGrid };
