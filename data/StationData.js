// StationData.js - QML Compatible Version with Railway Control System Data
.pragma library

// Core station layout data - ported from your firebase_test.py
var trackSegments = [
    // T1 Track segments
    { id: "T1S1",  startRow: 110, startCol: 0, endRow: 110, endCol: 12, occupied: false },
    { id: "T1S2",  startRow: 110, startCol: 13, endRow: 110, endCol: 34, occupied: false },
    { id: "T1S3",  startRow: 110, startCol: 35, endRow: 110, endCol: 67, occupied: false },
    { id: "T1S4",  startRow: 110, startCol: 68, endRow: 110, endCol: 90, occupied: false },
    { id: "T1S5",  startRow: 110, startCol: 101, endRow: 110, endCol: 153, occupied: false },
    { id: "T1S6",  startRow: 110, startCol: 154, endRow: 110, endCol: 232, occupied: false },
    { id: "T1S7",  startRow: 110, startCol: 233, endRow: 110, endCol: 277, occupied: false },
    { id: "T1S8",  startRow: 110, startCol: 287, endRow: 110, endCol: 305, occupied: false },
    { id: "T1S9",  startRow: 110, startCol: 306, endRow: 110, endCol: 338, occupied: false },
    { id: "T1S10", startRow: 110, startCol: 339, endRow: 110, endCol: 358, occupied: false },
    { id: "T1S11", startRow: 110, startCol: 359, endRow: 110, endCol: 369, occupied: false },

    // T4 Track segments
    { id: "T4S1", startRow: 88, startCol: 98, endRow: 88, endCol: 110, occupied: false },
    { id: "T4S2", startRow: 88, startCol: 120, endRow: 88, endCol: 255, occupied: false },
    { id: "T4S3", startRow: 88, startCol: 265, endRow: 88, endCol: 281, occupied: false },

    // Connection tracks
    { id: "T5S1", startRow: 106, startCol: 98, endRow: 92, endCol: 112, occupied: false },
    { id: "T6S1", startRow: 92, startCol: 263, endRow: 105, endCol: 277, occupied: false },
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
        row: 103,
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
        row: 113,
        col: 125,
        isActive: true,
        location: "Platform_A_Main_Departure"
    },
];

var pointMachines = [
    {
        id: "PM001",
        name: "Junction A",
        type: "POINT_MACHINE",

        // **JUNCTION GEOMETRY** - Where all tracks meet
        junctionPoint: { row: 110, col: 94.2 },

        // **TRACK CONNECTIONS** - Define relationships with offsets
        rootTrack: {
            trackId: "T1S4",           // The "common" track
            connectionEnd: "END",       // Which end connects to junction
            offset: { row: 0, col: 0 } // ✅ NEW: Offset for proper alignment
        },
        normalTrack: {                 // Straight-through path
            trackId: "T1S5",
            connectionEnd: "START",
            offset: { row: 0, col: 0 } // ✅ NEW: Offset to align with track thickness
        },
        reverseTrack: {                // Diverging path
            trackId: "T5S1",
            connectionEnd: "START",
            offset: { row: 0, col: 0 } // ✅ NEW: Offset for diagonal alignment
        },

        // **OPERATIONAL STATE**
        position: "REVERSE",            // "NORMAL" or "REVERSE"
        operatingStatus: "CONNECTED",  // "CONNECTED" or "IN_TRANSITION"

        // **PHYSICAL PROPERTIES**
        location: { row: 110, col: 95 },
        motorPosition: "BELOW",        // Visual placement of motor indicator
    },
    {
        id: "PM002",
        name: "Junction B",
        type: "POINT_MACHINE",

        // **JUNCTION GEOMETRY** - Where all tracks meet
        junctionPoint: { row: 88, col: 116 },

        // **TRACK CONNECTIONS** - Define relationships with offsets
        rootTrack: {
            trackId: "T4S2",           // The "common" track
            connectionEnd: "START",    // Which end connects to junction
            offset: { row: 0, col: 0 } //  NEW: Offset for proper alignment
        },
        normalTrack: {                 // Straight-through path
            trackId: "T4S1",
            connectionEnd: "END",
            offset: { row: 0, col: 0 } //  NEW: Offset to align with track thickness
        },
        reverseTrack: {                // Diverging path
            trackId: "T5S1",
            connectionEnd: "END",
            offset: { row: 0, col: 0 } //  NEW: Offset for diagonal alignment
        },

        // **OPERATIONAL STATE**
        position: "REVERSE",            // "NORMAL" or "REVERSE"
        operatingStatus: "CONNECTED",  // "CONNECTED" or "IN_TRANSITION"

        // **PHYSICAL PROPERTIES**
        location: { row: 88, col: 114 },
        motorPosition: "BELOW",        // Visual placement of motor indicator
    },
    {
        id: "PM003",
        name: "Junction C",
        type: "POINT_MACHINE",

        // **JUNCTION GEOMETRY** - Where all tracks meet
        junctionPoint: { row: 88, col: 258.7 },

        // **TRACK CONNECTIONS** - Define relationships with offsets
        rootTrack: {
            trackId: "T4S2",           // The "common" track
            connectionEnd: "END",    // Which end connects to junction
            offset: { row: 0, col: 0 } //  NEW: Offset for proper alignment
        },
        normalTrack: {                 // Straight-through path
            trackId: "T4S3",
            connectionEnd: "START",
            offset: { row: 0, col: 0 } //  NEW: Offset to align with track thickness
        },
        reverseTrack: {                // Diverging path
            trackId: "T6S1",
            connectionEnd: "START",
            offset: { row: 0, col: 0 } //  NEW: Offset for diagonal alignment
        },

        // **OPERATIONAL STATE**
        position: "REVERSE",            // "NORMAL" or "REVERSE"
        operatingStatus: "CONNECTED",  // "CONNECTED" or "IN_TRANSITION"

        // **PHYSICAL PROPERTIES**
        location: { row: 88, col: 261 },
        motorPosition: "BELOW",        // Visual placement of motor indicator
    },
    {
        id: "PM004",
        name: "Junction D",
        type: "POINT_MACHINE",

        // **JUNCTION GEOMETRY** - Where all tracks meet
        junctionPoint: { row: 110, col: 282.5 },

        // **TRACK CONNECTIONS** - Define relationships with offsets
        rootTrack: {
            trackId: "T1S8",           // The "common" track
            connectionEnd: "START",    // Which end connects to junction
            offset: { row: 0, col: 0 } //  NEW: Offset for proper alignment
        },
        normalTrack: {                 // Straight-through path
            trackId: "T1S7",
            connectionEnd: "END",
            offset: { row: 0, col: 0 } //  NEW: Offset to align with track thickness
        },
        reverseTrack: {                // Diverging path
            trackId: "T6S1",
            connectionEnd: "END",
            offset: { row: 0, col: 0 } //  NEW: Offset for diagonal alignment
        },

        // **OPERATIONAL STATE**
        position: "REVERSE",            // "NORMAL" or "REVERSE"
        operatingStatus: "CONNECTED",  // "CONNECTED" or "IN_TRANSITION"

        // **PHYSICAL PROPERTIES**
        location: { row: 110, col: 281 },
        motorPosition: "BELOW",        // Visual placement of motor indicator
    },
];

// Add this to the data arrays section
var advanceStarterSignals = [
    {
        id: "AS001",
        name: "Advanced Starter A1",
        type: "ADVANCED_STARTER",
        aspectCount: 2,
        possibleAspects: ["RED", "GREEN"],
        currentAspect: "RED",
        direction: "UP",
        row: 102,
        col: 302,
        isActive: true,
        location: "Advanced_Departure_A"
    },
    {
        id: "AS002",
        name: "Advanced Starter A2",
        type: "ADVANCED_STARTER",
        aspectCount: 2,
        possibleAspects: ["RED", "GREEN"],
        currentAspect: "GREEN",
        direction: "DOWN",
        row: 115,
        col: 56,
        isActive: true,
        location: "Advanced_Departure_B"
    },
];


// ============================================================================
// OUTER SIGNAL FUNCTIONS
// ============================================================================

/**
 * Retrieves an outer signal by its unique identifier
 * @param {String} signalId - Unique identifier for the outer signal (e.g., "OT001")
 * @returns {Object|undefined} - Outer signal object or undefined if not found
 * @example
 * var signal = getOuterSignalById("OT001");
 * if (signal) console.log("Found signal:", signal.name);
 */
function getOuterSignalById(signalId) {
    return outerSignals.find(signal => signal.id === signalId);
}

/**
 * Returns all outer signals in the system
 * @returns {Array} - Array of all outer signal objects
 * @example
 * var allOuters = getAllOuterSignals();
 * console.log("Total outer signals:", allOuters.length);
 */
function getAllOuterSignals() {
    return outerSignals;
}

/**
 * Filters outer signals by their directional operation
 * @param {String} direction - Direction filter ("UP" for incoming trains, "DOWN" for outgoing trains)
 * @returns {Array} - Array of outer signal objects matching the direction
 * @example
 * var upSignals = getOuterSignalsByDirection("UP");
 * var downSignals = getOuterSignalsByDirection("DOWN");
 */
function getOuterSignalsByDirection(direction) {
    return outerSignals.filter(signal => signal.direction === direction);
}

/**
 * Validates if an aspect change is permitted for an outer signal
 * @param {String} signalId - Unique identifier of the outer signal
 * @param {String} newAspect - Proposed new aspect ("RED", "SINGLE_YELLOW", "DOUBLE_YELLOW", "GREEN")
 * @returns {Boolean} - True if aspect change is valid, false otherwise
 * @example
 * if (isValidOuterAspectChange("OT001", "GREEN")) {
 *     // Proceed with aspect change
 * }
 */
function isValidOuterAspectChange(signalId, newAspect) {
    const signal = getOuterSignalById(signalId);
    if (!signal) return false;
    return signal.possibleAspects.includes(newAspect);
}

/**
 * Returns the standard color code for an outer signal aspect
 * @param {String} aspect - Signal aspect ("RED", "SINGLE_YELLOW", "DOUBLE_YELLOW", "GREEN")
 * @returns {String} - Hexadecimal color code for the aspect
 * @example
 * var redColor = getOuterAspectColor("RED"); // Returns "#e53e3e"
 * var greenColor = getOuterAspectColor("GREEN"); // Returns "#38a169"
 */
function getOuterAspectColor(aspect) {
    switch(aspect) {
        case "RED": return "#e53e3e";
        case "SINGLE_YELLOW": return "#d69e2e";
        case "DOUBLE_YELLOW": return "#f6ad55";
        case "GREEN": return "#38a169";
        default: return "#e53e3e";
    }
}

// ============================================================================
// HOME SIGNAL FUNCTIONS
// ============================================================================

/**
 * Retrieves a home signal by its unique identifier
 * @param {String} signalId - Unique identifier for the home signal (e.g., "HM001")
 * @returns {Object|undefined} - Home signal object or undefined if not found
 * @example
 * var homeSignal = getHomeSignalById("HM001");
 * if (homeSignal) console.log("Calling-on aspect:", homeSignal.callingOnAspect);
 */
function getHomeSignalById(signalId) {
    return homeSignals.find(signal => signal.id === signalId);
}

/**
 * Returns all home signals in the system
 * @returns {Array} - Array of all home signal objects
 * @example
 * var allHomes = getAllHomeSignals();
 * allHomes.forEach(signal => console.log(signal.name, signal.currentAspect));
 */
function getAllHomeSignals() {
    return homeSignals;
}

/**
 * Filters home signals by their directional operation
 * @param {String} direction - Direction filter ("UP" for platform entry, "DOWN" for platform exit)
 * @returns {Array} - Array of home signal objects matching the direction
 * @example
 * var entrySignals = getHomeSignalsByDirection("UP");
 * var exitSignals = getHomeSignalsByDirection("DOWN");
 */
function getHomeSignalsByDirection(direction) {
    return homeSignals.filter(signal => signal.direction === direction);
}

/**
 * Validates if a main aspect change is permitted for a home signal
 * @param {String} signalId - Unique identifier of the home signal
 * @param {String} newAspect - Proposed new main aspect ("RED", "YELLOW", "GREEN")
 * @returns {Boolean} - True if aspect change is valid, false otherwise
 * @example
 * if (isValidHomeAspectChange("HM001", "GREEN")) {
 *     // Check calling-on and loop signal states before proceeding
 * }
 */
function isValidHomeAspectChange(signalId, newAspect) {
    const signal = getHomeSignalById(signalId);
    if (!signal) return false;
    return signal.possibleAspects.includes(newAspect);
}

// ============================================================================
// STARTER SIGNAL FUNCTIONS
// ============================================================================

/**
 * Retrieves a starter signal by its unique identifier
 * @param {String} signalId - Unique identifier for the starter signal (e.g., "ST001")
 * @returns {Object|undefined} - Starter signal object or undefined if not found
 * @example
 * var starter = getStarterSignalById("ST001");
 * if (starter) console.log("Aspect count:", starter.aspectCount);
 */
function getStarterSignalById(signalId) {
    return starterSignals.find(signal => signal.id === signalId);
}

/**
 * Returns all starter signals in the system
 * @returns {Array} - Array of all starter signal objects
 * @example
 * var allStarters = getAllStarterSignals();
 * var twoAspectStarters = allStarters.filter(s => s.aspectCount === 2);
 */
function getAllStarterSignals() {
    return starterSignals;
}

/**
 * Filters starter signals by their directional operation
 * @param {String} direction - Direction filter ("UP" for loop entry, "DOWN" for main departure)
 * @returns {Array} - Array of starter signal objects matching the direction
 * @example
 * var departureSignals = getStarterSignalsByDirection("DOWN");
 * var loopSignals = getStarterSignalsByDirection("UP");
 */
function getStarterSignalsByDirection(direction) {
    return starterSignals.filter(signal => signal.direction === direction);
}

/**
 * Validates if an aspect change is permitted for a starter signal
 * @param {String} signalId - Unique identifier of the starter signal
 * @param {String} newAspect - Proposed new aspect ("RED", "YELLOW", "GREEN" - depending on aspectCount)
 * @returns {Boolean} - True if aspect change is valid, false otherwise
 * @example
 * if (isValidStarterAspectChange("ST001", "YELLOW")) {
 *     // 2-aspect starter can only show RED or YELLOW
 * }
 */
function isValidStarterAspectChange(signalId, newAspect) {
    const signal = getStarterSignalById(signalId);
    if (!signal) return false;
    return signal.possibleAspects.includes(newAspect);
}

// ============================================================================
// TRACK MANAGEMENT FUNCTIONS
// ============================================================================

/**
 * Resolves which physical point of a track connects to a junction
 * @param {Object} track - Track segment object containing startRow, startCol, endRow, endCol
 * @param {String} connectionEnd - "START" or "END" indicating which end to use
 * @returns {Object} - { row, col } coordinates of the connection point
 * @example
 * var track = getTrackById("T1S1");
 * var startPoint = getTrackEndpoint(track, "START");
 * var endPoint = getTrackEndpoint(track, "END");
 */
function getTrackEndpoint(track, connectionEnd) {
    if (connectionEnd === "START") {
        return {
            row: track.startRow,
            col: track.startCol
        };
    } else { // "END"
        return {
            row: track.endRow,
            col: track.endCol
        };
    }
}

/**
 * Finds a track object by its unique identifier
 * @param {String} trackId - Track identifier (e.g., "T1S1", "T4S2")
 * @returns {Object|undefined} - Track object or undefined if not found
 * @example
 * var track = getTrackById("T1S1");
 * if (track) console.log("Track length:", track.endCol - track.startCol);
 */
function getTrackById(trackId) {
    return trackSegments.find(track => track.id === trackId);
}

// ============================================================================
// POINT MACHINE FUNCTIONS
// ============================================================================

/**
 * Retrieves a point machine by its unique identifier
 * @param {String} machineId - Unique identifier for the point machine (e.g., "PM001")
 * @returns {Object|undefined} - Point machine object or undefined if not found
 * @example
 * var pm = getPointMachineById("PM001");
 * if (pm) console.log("Current position:", pm.position);
 */
function getPointMachineById(machineId) {
    return pointMachines.find(pm => pm.id === machineId);
}

/**
 * Returns all point machines in the system
 * @returns {Array} - Array of all point machine objects
 * @example
 * var allPMs = getAllPointMachines();
 * var activePMs = allPMs.filter(pm => pm.operatingStatus === "CONNECTED");
 */
function getAllPointMachines() {
    return pointMachines;
}

/**
 * Determines which track is currently active (connected) for a point machine
 * @param {Object} pointMachine - Point machine object
 * @returns {String} - Active track ID (either normal or reverse track)
 * @example
 * var pm = getPointMachineById("PM001");
 * var activeTrack = getConnectedTrackId(pm);
 * console.log("Currently connected to track:", activeTrack);
 */
function getConnectedTrackId(pointMachine) {
    if (pointMachine.position === "NORMAL") {
        return pointMachine.normalTrack.trackId;
    } else {
        return pointMachine.reverseTrack.trackId;
    }
}

/**
 * Finds the point machine that controls a specific track
 * @param {String} trackId - Track identifier to search for
 * @returns {Object|null} - Point machine object that controls this track, or null if none found
 * @example
 * var pm = getPointMachineByTrack("T1S2");
 * if (pm) console.log("Track T1S2 is controlled by:", pm.name);
 */
function getPointMachineByTrack(trackId) {
    return pointMachines.find(pm =>
        pm.rootTrack.trackId === trackId ||
        pm.normalTrack.trackId === trackId ||
        pm.reverseTrack.trackId === trackId
    );
}

/**
 * Checks if a point machine operation is safe to perform
 * @param {Object} pointMachine - Point machine object to check
 * @returns {Boolean} - True if safe to operate, false if blocked by occupied tracks
 * @example
 * var pm = getPointMachineById("PM001");
 * if (isPointMachineOperationSafe(pm)) {
 *     operatePointMachine("PM001", "REVERSE");
 * }
 */
function isPointMachineOperationSafe(pointMachine) {
    // Check if any controlled tracks are occupied
    var tracks = [
        pointMachine.rootTrack.trackId,
        pointMachine.normalTrack.trackId,
        pointMachine.reverseTrack.trackId
    ];

    return !tracks.some(trackId => {
        var track = getTrackById(trackId);
        return track && track.occupied;
    });
}

// data/StationData.js - Replace the operatePointMachine function

/**
 * Attempts to start a point machine operation to a new position
 * @param {String} machineId - Unique identifier of the point machine
 * @param {String} newPosition - Desired position ("NORMAL" or "REVERSE")
 * @returns {Object} - Operation result with success status and transition time
 * @example
 * var result = operatePointMachine("PM001", "REVERSE");
 * if (result.success) console.log("Transition time:", result.transitionTime);
 */
function operatePointMachine(machineId, newPosition) {
    var pm = getPointMachineById(machineId);
    if (!pm || pm.operatingStatus === "IN_TRANSITION") {
        return {
            success: false,
            reason: "Cannot operate during transition or machine not found",
            transitionTime: 0
        };
    }

    if (pm.position === newPosition) {
        return {
            success: true,
            reason: "Already in desired position",
            transitionTime: 0
        };
    }

    // Safety checks
    if (!isPointMachineOperationSafe(pm)) {
        console.warn("Point machine operation blocked - safety interlock");
        return {
            success: false,
            reason: "Safety interlock - tracks occupied",
            transitionTime: 0
        };
    }

    // Begin transition
    // THE TRAMSITION DURATION IS FOR DEMONSTRATION PURPOSES ONLY. UNDER REAL ENVIRONMENT THIS WILL BE REMOVED
    pm.operatingStatus = "IN_TRANSITION";
    console.log("Point machine", machineId, "starting transition to", newPosition);

    return {
        success: true,
        reason: "Transition started",
        transitionTime: pm.transitionTime || 3000,
        targetPosition: newPosition
    };
}

/**
 * Completes a point machine transition (called after timer expires)
 * @param {String} machineId - Unique identifier of the point machine
 * @param {String} newPosition - Target position to set
 * @returns {Boolean} - True if completion was successful
 */
function completePointMachineOperation(machineId, newPosition) {
    var pm = getPointMachineById(machineId);
    if (!pm) {
        console.error("Point machine not found:", machineId);
        return false;
    }

    pm.position = newPosition;
    pm.operatingStatus = "CONNECTED";
    console.log("Point machine", machineId, "operation complete:", newPosition);
    return true;
}

// ============================================================================
// ADVANCED STARTER SIGNAL FUNCTIONS
// ============================================================================

/**
 * Retrieves an advanced starter signal by its unique identifier
 * @param {String} signalId - Unique identifier for the advanced starter signal (e.g., "AS001")
 * @returns {Object|undefined} - Advanced starter signal object or undefined if not found
 * @example
 * var signal = getAdvanceStarterSignalById("AS001");
 * if (signal) console.log("Found signal:", signal.name);
 */
function getAdvanceStarterSignalById(signalId) {
    return advanceStarterSignals.find(signal => signal.id === signalId);
}

/**
 * Returns all advanced starter signals in the system
 * @returns {Array} - Array of all advanced starter signal objects
 * @example
 * var allAdvanced = getAllAdvanceStarterSignals();
 * console.log("Total advanced starter signals:", allAdvanced.length);
 */
function getAllAdvanceStarterSignals() {
    return advanceStarterSignals;
}

/**
 * Filters advanced starter signals by their directional operation
 * @param {String} direction - Direction filter ("UP" or "DOWN")
 * @returns {Array} - Array of advanced starter signal objects matching the direction
 * @example
 * var upSignals = getAdvanceStarterSignalsByDirection("UP");
 * var downSignals = getAdvanceStarterSignalsByDirection("DOWN");
 */
function getAdvanceStarterSignalsByDirection(direction) {
    return advanceStarterSignals.filter(signal => signal.direction === direction);
}

/**
 * Validates if an aspect change is permitted for an advanced starter signal
 * @param {String} signalId - Unique identifier of the advanced starter signal
 * @param {String} newAspect - Proposed new aspect ("RED" or "GREEN")
 * @returns {Boolean} - True if aspect change is valid, false otherwise
 * @example
 * if (isValidAdvanceStarterAspectChange("AS001", "GREEN")) {
 *     // Proceed with aspect change
 * }
 */
function isValidAdvanceStarterAspectChange(signalId, newAspect) {
    const signal = getAdvanceStarterSignalById(signalId);
    if (!signal) return false;
    return signal.possibleAspects.includes(newAspect);
}

/**
 * Returns the standard color code for an advanced starter signal aspect
 * @param {String} aspect - Signal aspect ("RED" or "GREEN")
 * @returns {String} - Hexadecimal color code for the aspect
 * @example
 * var redColor = getAdvanceStarterAspectColor("RED"); // Returns "#ff0000"
 * var greenColor = getAdvanceStarterAspectColor("GREEN"); // Returns "#00ff00"
 */
function getAdvanceStarterAspectColor(aspect) {
    switch(aspect) {
        case "RED": return "#ff0000";
        case "GREEN": return "#00ff00";
        default: return "#ff0000"; // Safe default to RED
    }
}

// ============================================================================
// COORDINATE CONVERSION UTILITIES
// ============================================================================

/**
 * Converts grid coordinates to pixel coordinates
 * @param {Number} row - Grid row position
 * @param {Number} col - Grid column position
 * @param {Number} cellSize - Size of each grid cell in pixels
 * @returns {Object} - { x, y } pixel coordinates
 * @example
 * var pixelPos = gridToPixel(10, 20, 15);
 * console.log("Pixel position:", pixelPos.x, pixelPos.y);
 */
function gridToPixel(row, col, cellSize) {
    return {
        x: col * cellSize,
        y: row * cellSize
    };
}

/**
 * Converts pixel coordinates to grid coordinates
 * @param {Number} x - Pixel X coordinate
 * @param {Number} y - Pixel Y coordinate
 * @param {Number} cellSize - Size of each grid cell in pixels
 * @returns {Object} - { row, col } grid coordinates
 * @example
 * var gridPos = pixelToGrid(150, 300, 15);
 * console.log("Grid position:", gridPos.row, gridPos.col);
 */
function pixelToGrid(x, y, cellSize) {
    return {
        row: Math.floor(y / cellSize),
        col: Math.floor(x / cellSize)
    };
}
