// StationData.js - QML Compatible Version with 50-row offset
.pragma library

// Core station layout data - ported from your firebase_test.py
var trackSegments = [
    // T1 Track segments
    { id: "T1S1",  startRow: 110, startCol: 0, endRow: 110, endCol: 12, occupied: false },
    { id: "T1S2",  startRow: 110, startCol: 13, endRow: 110, endCol: 34, occupied: false },
    { id: "T1S3",  startRow: 110, startCol: 35, endRow: 110, endCol: 67, occupied: false },
    { id: "T1S4",  startRow: 110, startCol: 68, endRow: 110, endCol: 95, occupied: false },
    { id: "T1S5",  startRow: 110, startCol: 99, endRow: 110, endCol: 153, occupied: false },
    { id: "T1S6",  startRow: 110, startCol: 154, endRow: 110, endCol: 232, occupied: false },
    { id: "T1S7",  startRow: 110, startCol: 233, endRow: 110, endCol: 277, occupied: false },
    { id: "T1S8",  startRow: 110, startCol: 281, endRow: 110, endCol: 305, occupied: false },
    { id: "T1S9",  startRow: 110, startCol: 306, endRow: 110, endCol: 338, occupied: false },
    { id: "T1S10", startRow: 110, startCol: 339, endRow: 110, endCol: 358, occupied: false },
    { id: "T1S11", startRow: 110, startCol: 359, endRow: 110, endCol: 369, occupied: false },

    // T4 Track segments
    { id: "T4S1", startRow: 88, startCol: 100, endRow: 88, endCol: 112, occupied: false },
    { id: "T4S2", startRow: 88, startCol: 116, endRow: 88, endCol: 259, occupied: false },
    { id: "T4S3", startRow: 88, startCol: 263, endRow: 88, endCol: 270, occupied: false },

    // Connection tracks
    { id: "T5S1", startRow: 107, startCol: 97, endRow: 92, endCol: 112, occupied: false },
    { id: "T6S1", startRow: 92, startCol: 263, endRow: 107, endCol: 279, occupied: false },
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
    { text: "T1S1", row: 107, col: 7, fontSize: 12 },
    { text: "T1S2", row: 107, col: 24, fontSize: 12 },
    { text: "T1S3", row: 107, col: 51, fontSize: 12 },
    { text: "T1S4", row: 107, col: 75, fontSize: 12 },
    { text: "T1S5", row: 107, col: 124, fontSize: 12 },
    { text: "T1S6", row: 107, col: 193, fontSize: 12 },
    { text: "T1S7", row: 107, col: 256, fontSize: 12 },
    { text: "T1S8", row: 107, col: 290, fontSize: 12 },
    { text: "T1S9", row: 107, col: 318, fontSize: 12 },
    { text: "T1S10", row: 107, col: 346, fontSize: 12 },
    { text: "T1S11", row: 107, col: 360, fontSize: 12 },


    { text: "T4S1", row: 85, col: 105, fontSize: 12 },
];

var outerSignals = [
    {
        id: "OT001",
        name: "Outer A1",
        type: "OUTER",
        aspectCount: 4,
        possibleAspects: ["RED", "SINGLE_YELLOW", "DOUBLE_YELLOW", "GREEN"],
        currentAspect: "DOUBLE_YELLOW",
        direction: "UP",
        row: 102,
        col: 30,
        isActive: true,
        location: "Approach_Block_1"
    },
    {
        id: "OT002",
        name: "Outer A2",
        type: "OUTER",
        aspectCount: 4,
        possibleAspects: ["RED", "SINGLE_YELLOW", "DOUBLE_YELLOW", "GREEN"],
        currentAspect: "DOUBLE_YELLOW",
        direction: "DOWN",
        row: 115,
        col: 330,
        isActive: true,
        location: "Approach_Block_2"
    },
];

var homeSignals = [
    {
        id: "HM001",
        name: "Home A1",
        type: "HOME",
        aspectCount: 3,
        possibleAspects: ["RED", "YELLOW", "GREEN"],
        currentAspect: "RED",
        callingOnAspect: "DARK",
        loopAspect: "YELLOW",
        loopSignalConfiguration: "UR",
        direction: "UP",
        row: 102,
        col: 82,
        isActive: true,
        location: "Platform_A_Entry"
    },
    {
        id: "HM002",
        name: "Home A2",
        type: "HOME",
        aspectCount: 3,
        possibleAspects: ["RED", "YELLOW", "GREEN"],
        currentAspect: "GREEN",
        callingOnAspect: "WHITE",
        loopAspect: "ON",
        loopSignalConfiguration: "UR",
        direction: "DOWN",
        row: 115,
        col: 270,
        isActive: true,
        location: "Platform_A_Exit"
    },
];

var starterSignals = [
    {
        id: "ST001",
        name: "Starter A1",
        type: "STARTER",
        aspectCount: 2,
        possibleAspects: ["RED", "YELLOW"],
        currentAspect: "RED",
        direction: "UP",
        row: 83,
        col: 245,
        isActive: true,
        location: "Platform_A_Departure"
    },
    {
        id: "ST002",
        name: "Starter A2",
        type: "STARTER",
        aspectCount: 3,
        possibleAspects: ["RED", "YELLOW", "GREEN"],
        currentAspect: "YELLOW",
        direction: "UP",
        row: 105,
        col: 245,
        isActive: true,
        location: "Platform_A_Main_Departure"
    },
    {
        id: "ST003",
        name: "Starter B1",
        type: "STARTER",
        aspectCount: 2,
        possibleAspects: ["RED", "YELLOW"],
        currentAspect: "RED",
        direction: "DOWN",
        row: 91,
        col: 125,
        isActive: true,
        location: "Junction_Loop_Entry"
    },
    {
        id: "ST004",
        name: "Starter B2",
        type: "STARTER",
        aspectCount: 3,
        possibleAspects: ["RED", "YELLOW", "GREEN"],
        currentAspect: "YELLOW",
        direction: "DOWN",
        row: 115,
        col: 125,
        isActive: true,
        location: "Platform_A_Main_Departure"
    },
];

var levelCrossings = [
    { id: "LC001", row: 87, col: 36, state: "OPEN", name: "LC_GATE1" },
    { id: "LC002", row: 87, col: 211, state: "OPEN", name: "LC_GATE2" }
];

function getOuterSignalById(signalId) {
    return outerSignals.find(signal => signal.id === signalId);
}
function getAllOuterSignals() {
    return outerSignals;
}
function getOuterSignalsByDirection(direction) {
    return outerSignals.filter(signal => signal.direction === direction);
}
function isValidOuterAspectChange(signalId, newAspect) {
    const signal = getOuterSignalById(signalId);
    if (!signal) return false;
    return signal.possibleAspects.includes(newAspect);
}
function getOuterAspectColor(aspect) {
    switch(aspect) {
        case "RED": return "#e53e3e";
        case "SINGLE_YELLOW": return "#d69e2e";
        case "DOUBLE_YELLOW": return "#f6ad55";
        case "GREEN": return "#38a169";
        default: return "#e53e3e";
    }
}
function getHomeSignalById(signalId) {
    return homeSignals.find(signal => signal.id === signalId);
}
function getAllHomeSignals() {
    return homeSignals;
}
function getHomeSignalsByDirection(direction) {
    return homeSignals.filter(signal => signal.direction === direction);
}
function isValidHomeAspectChange(signalId, newAspect) {
    const signal = getHomeSignalById(signalId);
    if (!signal) return false;
    return signal.possibleAspects.includes(newAspect);
}
function getStarterSignalById(signalId) {
    return starterSignals.find(signal => signal.id === signalId);
}
function getAllStarterSignals() {
    return starterSignals;
}
function getStarterSignalsByDirection(direction) {
    return starterSignals.filter(signal => signal.direction === direction);
}
function isValidStarterAspectChange(signalId, newAspect) {
    const signal = getStarterSignalById(signalId);
    if (!signal) return false;
    return signal.possibleAspects.includes(newAspect);
}
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
