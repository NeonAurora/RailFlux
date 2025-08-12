# RailFlux Railway Control System - Architectural Context

## Table of Contents
1. [Project Overview](#project-overview)
2. [System Architecture](#system-architecture)
3. [Database Design](#database-design)
4. [Application Startup & Initialization](#application-startup--initialization)
5. [Core Components Deep Dive](#core-components-deep-dive)
6. [QML Frontend Architecture](#qml-frontend-architecture)
7. [Safety & Interlocking System](#safety--interlocking-system)
8. [Data Flow & Signal Patterns](#data-flow--signal-patterns)
9. [Performance & Real-time Considerations](#performance--real-time-considerations)
10. [Safety-Critical Features](#safety-critical-features)

---

## Project Overview

RailFlux is a safety-critical railway control system built with Qt6/QML and C++20, designed for 24/7 operation in railway control environments. The system provides real-time monitoring and control of railway signals, track circuits, and point machines with comprehensive safety interlocking.

### Key Characteristics
- **Safety-Critical**: All operations validated through interlocking rules
- **Real-time**: Sub-50ms response time targets for safety operations
- **Resilient**: Automatic database recovery, connection management
- **Professional**: Industrial HMI design standards
- **Modular**: Layered architecture with clear separation of concerns

---

## System Architecture
┌──────────────────────────────────────────────────────────────────────────────┐
│            Railway Interlocking System — High-Level Architecture             │
├──────────────────────────────────────────────────────────────────────────────┤
│                                QML Frontend                                  │
│  ┌──────────────┬──────────────┬─────────────────┬─────────────────────────┐ │
│  │  Main.qml    │ StationLayout│  Signal/Track   │  Toast / Context Menu   │ │
│  │              │   (views)    │   Components    │    (UX helpers)         │ │
│  └──────────────┴──────────────┴─────────────────┴─────────────────────────┘ │
│                                   ▲     ▲                                    │
│                      Qt signals/slots & Q_INVOKABLE                          │
│                                   ▼     ▼                                    │
├──────────────────────────────────────────────────────────────────────────────┤
│                             C++ Business Logic                               │
│  ┌──────────────────────────────────────────────────────────────────────────┐│
│  │                           InterlockingService                            ││
│  │  ┌──────────────┬──────────────┬────────────────────┐                    ││
│  │  │ SignalBranch │ TrackCircuit │ PointMachineBranch │                    ││
│  │  │              │    Branch    │                    │                    ││
│  │  └──────────────┴──────────────┴────────────────────┘                    ││
│  │  ┌────────────────────────────────────────────────────────────────────┐  ││
│  │  │                      InterlockingRuleEngine                        │  ││
│  │  └────────────────────────────────────────────────────────────────────┘  ││
│  └──────────────────────────────────────────────────────────────────────────┘│
├──────────────────────────────────────────────────────────────────────────────┤
│                               Database Layer                                 │
│  ┌──────────────────────────┬───────────────────────────────────────────────┐│
│  │     DatabaseManager      │            DatabaseInitializer                ││
│  │  • Connection Mgmt       │  • Schema Creation                            ││
│  │  • Real-time Polling     │  • Data Population                            ││
│  │  • LISTEN / NOTIFY       │  • Validation                                 ││
│  └──────────────────────────┴───────────────────────────────────────────────┘│
├──────────────────────────────────────────────────────────────────────────────┤
│                            PostgreSQL Database                               │
│  ┌──────────────────────┬───────────────────┬──────────────────────────────┐ │
│  │ railway_control      │ railway_audit     │ railway_config               │ │
│  │ • operational data   │ • event logs      │ • lookup tables              │ │
│  │                      │ • audit trail     │ • signal types / aspects     │ │
│  │                      │                   │ • point positions            │ │
│  └──────────────────────┴───────────────────┴──────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────┘

### Architecture Layers

#### 1. **QML Presentation Layer**
- **Framework**: Qt6 Quick/QML
- **Responsibility**: User interface, visualization, user interaction
- **Key Files**: `Main.qml`, `layouts/StationLayout.qml`, `components/*.qml`

#### 2. **C++ Business Logic Layer**
- **Framework**: C++20 with Qt6 Core/Sql
- **Responsibility**: Safety validation, business rules, database operations
- **Key Classes**: `InterlockingService`, `DatabaseManager`, Branch classes

#### 3. **Database Persistence Layer**
- **Framework**: PostgreSQL with Qt SQL drivers
- **Responsibility**: Data storage, real-time notifications, audit trail
- **Schemas**: `railway_control`, `railway_audit`, `railway_config`

---

## Database Design

### Schema Architecture

The database uses a three-schema approach for clear separation of concerns:

```sql
-- Configuration & Lookup Tables
railway_config.*
├── signal_types          (Signal classification)
├── signal_aspects        (Color codes, safety levels)
└── point_positions       (NORMAL/REVERSE definitions)

-- Operational Data  
railway_control.*
├── track_circuits        (Hardware occupancy detection)
├── track_segments        (Logical track sections)
├── signals              (Signal state & configuration)
├── point_machines       (Point machine positions)
├── interlocking_rules   (Safety rule definitions)
└── system_state         (Application state storage)

-- Audit & Compliance
railway_audit.*
├── signal_operations    (Signal change history)
├── point_operations     (Point machine operations)
├── system_events        (Application events)
└── audit_summary        (Compliance reporting)
```

### Core Tables Structure

#### railway_control.signals

```sql
CREATE TABLE railway_control.signals (
    id SERIAL PRIMARY KEY,
    signal_id VARCHAR(20) NOT NULL UNIQUE,           -- Business key (e.g., "OS001")
    signal_name VARCHAR(100) NOT NULL,               -- Display name
    signal_type_id INTEGER REFERENCES signal_types,  -- OUTER/HOME/STARTER/etc.
    location_row NUMERIC(10,2) NOT NULL,            -- Grid position
    location_col NUMERIC(10,2) NOT NULL,
    direction VARCHAR(10) CHECK (direction IN ('UP', 'DOWN')),
    current_aspect_id INTEGER REFERENCES signal_aspects,
    aspect_count INTEGER NOT NULL DEFAULT 2,         -- 2,3,4 aspect signal
    possible_aspects TEXT[],                         -- Allowed aspects
    protected_track_segments TEXT[],                 -- Safety dependencies
    interlocked_with INTEGER[],                      -- Signal dependencies
    is_active BOOLEAN DEFAULT TRUE,
    -- Audit fields
    last_changed_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    last_changed_by VARCHAR(100)
);
```

#### railway_control.track_circuits

```sql
CREATE TABLE railway_control.track_circuits (
    circuit_id VARCHAR(20) PRIMARY KEY,             -- Hardware identifier
    circuit_name VARCHAR(100) NOT NULL,
    is_occupied BOOLEAN DEFAULT FALSE,              -- Hardware state
    occupied_by VARCHAR(50),                        -- Train identification
    last_occupied_at TIMESTAMP WITH TIME ZONE,
    -- Safety flags
    is_failed BOOLEAN DEFAULT FALSE,
    failure_reason TEXT
);
```
#### railway_control.track_segments

```sql
CREATE TABLE railway_control.track_segments (
    segment_id VARCHAR(20) PRIMARY KEY,             -- Business identifier
    segment_name VARCHAR(100) NOT NULL,
    start_row NUMERIC(10,2) NOT NULL,              -- Visual coordinates
    start_col NUMERIC(10,2) NOT NULL,
    end_row NUMERIC(10,2) NOT NULL,
    end_col NUMERIC(10,2) NOT NULL,
    circuit_id VARCHAR(20) REFERENCES track_circuits, -- Physical link
    is_assigned BOOLEAN DEFAULT FALSE,              -- Logical occupancy
    protecting_signals TEXT[],                      -- Safety references
    length_meters NUMERIC(10,2),
    max_speed_kmh INTEGER
);
```

### Database Functions & Procedures

#### The system uses PostgreSQL stored functions for atomic operations:

```sql
-- Signal aspect updates with audit trail
CREATE OR REPLACE FUNCTION update_signal_aspect(
    p_signal_id VARCHAR(20),
    p_new_aspect VARCHAR(20),
    p_operator VARCHAR(100)
) RETURNS BOOLEAN;

-- Point machine position changes
CREATE OR REPLACE FUNCTION update_point_position(
    p_machine_id VARCHAR(20),
    p_new_position VARCHAR(20),
    p_operator VARCHAR(100)
) RETURNS BOOLEAN;

-- Track circuit occupancy simulation
CREATE OR REPLACE FUNCTION simulate_track_occupancy(
    p_circuit_id VARCHAR(20),
    p_occupied BOOLEAN,
    p_train_id VARCHAR(50)
) RETURNS BOOLEAN;
```

### Application Startup & Initialization

#### main.cpp - Application Bootstrap

```sql
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    // 1. Create global C++ instances
    DatabaseManager* dbManager = new DatabaseManager(&app);
    DatabaseInitializer* dbInitializer = new DatabaseInitializer(&app);
    InterlockingService* interlockingService = new InterlockingService(dbManager, &app);

    // 2. Register with QML context
    engine.rootContext()->setContextProperty("globalDatabaseManager", dbManager);
    engine.rootContext()->setContextProperty("globalDatabaseInitializer", dbInitializer);
    engine.rootContext()->setContextProperty("globalInterlockingService", interlockingService);

    // 3. Connect interlocking service to database
    dbManager->setInterlockingService(interlockingService);

    // 4. Initialize interlocking when database connects
    QObject::connect(dbManager, &DatabaseManager::connectionStateChanged,
                     [interlockingService](bool connected) {
                         if (connected) {
                             interlockingService->initialize();
                         }
                     });

    // 5. Safety system monitoring
    QObject::connect(interlockingService, &InterlockingService::systemFreezeRequired,
                     [](const QString& trackSegmentId, const QString& reason, const QString& details) {
                         qCritical() << "🚨 SYSTEM FREEZE ACTIVATED 🚨";
                         qCritical() << "Track Segment:" << trackSegmentId;
                         qCritical() << "Reason:" << reason;
                         qCritical() << "Details:" << details;
                     });

    // 6. Load QML interface
    engine.loadFromModule("RailFlux", "Main");

    // 7. Start database connection and real-time updates
    if (dbManager->connectToDatabase()) {
        dbManager->startPolling();
        dbManager->enableRealTimeUpdates();
    }

    return app.exec();
}
```

### Initialization Sequence

```
App Start
    ↓
Create C++ Objects
    ↓
Register with QML
    ↓
Load QML Interface (Main.qml)
    ↓
Database Connection Attempt
    ├── System PostgreSQL (port 5432)
    └── Portable PostgreSQL (port 5433)
    ↓
Schema Validation/Creation
    ↓
Start Polling (400-500ms intervals)
    ↓
Enable LISTEN/NOTIFY
    ↓
Initialize Interlocking Service
    ↓
System Operational
```

## Core Components Deep Dive

### DatabaseManager Class

**File**: `database/DatabaseManager.h`, `database/DatabaseManager.cpp`

**Responsibilities**:
- Database connection management (system + portable PostgreSQL)
- Real-time data polling with configurable intervals
- LISTEN/NOTIFY for immediate updates
- CRUD operations with safety validation
- Connection resilience and automatic recovery

### Key Methods & Data Flow

```cpp
class DatabaseManager : public QObject {
    Q_OBJECT

    // === CONNECTION MANAGEMENT ===
    public slots:
        bool connectToDatabase();              // Try system, fallback to portable
        void startPolling();                   // Begin periodic data refresh
        void enableRealTimeUpdates();          // Subscribe to database notifications

    // === DATA RETRIEVAL (Direct DB Queries - No Caching) ===
    Q_INVOKABLE QVariantList getTrackSegmentsList();
    Q_INVOKABLE QVariantList getAllSignalsList();
    Q_INVOKABLE QVariantList getPointMachinesList();
    Q_INVOKABLE QVariantMap getSignalById(const QString& signalId);

    // === SAFETY-CRITICAL UPDATES ===
    Q_INVOKABLE bool updateSignalAspect(const QString& signalId, const QString& newAspect);
    Q_INVOKABLE bool updatePointMachinePosition(const QString& machineId, const QString& newPosition);

    // === REAL-TIME SIGNALS ===
    signals:
        void connectionStateChanged(bool connected);
        void signalUpdated(const QString& signalId);
        void trackSegmentUpdated(const QString& segmentId);
        void signalsChanged();                 // Batch update notification
        void operationBlocked(const QString& entityId, const QString& reason);

private:
    // Connection state
    bool connected = false;
    QSqlDatabase db;
    
    // Polling mechanism
    QTimer* pollingTimer;
    int pollingInterval = 500;                // Production: 400-500ms
    
    // Portable PostgreSQL support
    QProcess* m_postgresProcess;
    QString m_postgresPath;
    int m_portablePort = 5433;
    
    // Safety integration
    InterlockingService* m_interlockingService;
};
```

**Database Connection Strategy**:
- Primary: Attempt system PostgreSQL (localhost:5432)
- Fallback: Start portable PostgreSQL (localhost:5433)
- Initialization: Create schemas if missing
- Validation: Verify data integrity

### Safety-Critical Update Pattern

```cpp
bool DatabaseManager::updateSignalAspect(const QString& signalId, const QString& newAspect) {
    // 1. Get current state
    QString currentAspect = getCurrentSignalAspect(signalId);
    
    // 2. Interlocking validation
    if (m_interlockingService) {
        auto validation = m_interlockingService->validateSignalOperation(
            signalId, currentAspect, newAspect, "HMI_USER");
        
        if (!validation.isAllowed()) {
            emit operationBlocked(signalId, validation.getReason());
            return false;
        }
    }
    
    // 3. Database transaction
    if (!db.transaction()) return false;
    
    // 4. Execute stored procedure
    QSqlQuery query(db);
    query.prepare("SELECT railway_control.update_signal_aspect(?, ?, 'HMI_USER')");
    query.addBindValue(signalId);
    query.addBindValue(newAspect);
    
    // 5. Commit and verify
    bool success = query.exec() && query.next() && query.value(0).toBool();
    if (success && db.commit()) {
        emit signalUpdated(signalId);
        return true;
    } else {
        db.rollback();
        return false;
    }
}
```

## Interlocking Service Class

**File**: `interlocking/InterlockingService.h`, `interlocking/InterlockingService.cpp`

**Responsibilities**:
- Central safety validation coordinator
- Performance monitoring (target: <50ms response)
- Reactive safety enforcement for track occupancy
- System freeze activation for critical failures

### Architecture Pattern:

```cpp
class InterlockingService : public QObject {
    Q_OBJECT

public:
    // === VALIDATION API (Operator Actions) ===
    Q_INVOKABLE ValidationResult validateSignalOperation(
        const QString& signalId, 
        const QString& currentAspect,
        const QString& requestedAspect, 
        const QString& operatorId
    );
    
    Q_INVOKABLE ValidationResult validatePointMachineOperation(
        const QString& machineId,
        const QString& currentPosition,
        const QString& requestedPosition,
        const QString& operatorId
    );

    // === REACTIVE SAFETY (Hardware Events) ===
    void reactToTrackSegmentOccupancyChange(
        const QString& trackSegmentId, 
        bool wasOccupied, 
        bool isOccupied
    );

signals:
    // Safety system signals
    void systemFreezeRequired(const QString& trackSegmentId, const QString& reason, const QString& details);
    void operationBlocked(const QString& entityId, const QString& reason);
    void automaticProtectionActivated(const QString& trackSegmentId, const QString& details);

private:
    // Validation branches
    std::unique_ptr<SignalBranch> m_signalBranch;
    std::unique_ptr<TrackCircuitBranch> m_trackSegmentBranch;
    std::unique_ptr<PointMachineBranch> m_pointBranch;
    
    // Performance monitoring
    static constexpr double TARGET_RESPONSE_TIME_MS = 50.0;
    QList<double> m_responseHistory;
    
    DatabaseManager* m_dbManager;
    bool m_isOperational = false;
};
```

### Validation Branch Pattern

Each validation branch handles specific entity types:

**SignalBranch** - Signal Aspect Changes
**TrackCircuitBranch** - Track occupancy reactive protection
**PointMachineBranch** - Point machine position changes

```cpp
// Example: SignalBranch validation flow
ValidationResult SignalBranch::validateAspectChange(
    const QString& signalId,
    const QString& currentAspect, 
    const QString& requestedAspect,
    const QString& operatorId) {
    
    // 1. Signal state validation
    auto stateResult = checkSignalActive(signalId);
    if (!stateResult.isAllowed()) return stateResult;
    
    // 2. Aspect transition validation
    auto transitionResult = checkAspectTransition(currentAspect, requestedAspect);
    if (!transitionResult.isAllowed()) return transitionResult;
    
    // 3. Track protection validation (for proceed aspects)
    auto trackResult = checkTrackSegmentProtection(signalId, requestedAspect);
    if (!trackResult.isAllowed()) return trackResult;
    
    // 4. Interlocked signals validation
    auto interlockResult = checkInterlockedSignals(signalId, currentAspect, requestedAspect);
    if (!interlockResult.isAllowed()) return interlockResult;
    
    return ValidationResult::allowed("All validation checks passed");
}
```
## QML Frontend Architecture

### Main.qml - Application Shell

**File** - `Main.qml`

**Responsibilities**:
- Application window and theme definition
- Global signal handling and routing
- Database connection state management
- System freeze detection and user notification

#### Structure & Signal Flow

```cpp
ApplicationWindow {
    id: window
    
    // === THEME DEFINITION ===
    QtObject {
        id: theme
        readonly property color darkBackground: "#1a1a1a"
        readonly property color controlBackground: "#2d3748"
        readonly property color accentBlue: "#3182ce"
        readonly property color successGreen: "#38a169"
        readonly property color warningYellow: "#d69e2e"
        readonly property color dangerRed: "#e53e3e"
    }
    
    // === CONNECTION STATE MONITORING ===
    QtObject {
        id: connectionStatus
        property bool connected: false
        
        // Real-time connection state updates
        Connections {
            target: globalDatabaseManager
            function onConnectionStateChanged(isConnected) {
                connectionStatus.connected = isConnected
                if (isConnected) {
                    stationLayout.refreshAllData()
                }
            }
        }
    }
    
    // === SAFETY SYSTEM MONITORING ===
    Connections {
        target: globalInterlockingService
        
        // Critical system freeze detection
        function onSystemFreezeRequired(trackSegmentId, reason, details) {
            toastNotification.show(
                "🚨 SYSTEM FREEZE ACTIVATED",
                `Critical safety violation detected on ${trackSegmentId}. ${reason}. All operations suspended.`,
                trackSegmentId,
                details,
                false  // No auto-hide for critical alerts
            )
        }
        
        // Operation blocking notifications
        function onOperationBlocked(entityId, reason) {
            toastNotification.show(
                "❌ Operation Blocked",
                `${entityId}: ${reason}`,
                entityId,
                reason,
                true   // Auto-hide after timeout
            )
        }
    }
    
    // === MAIN LAYOUT ===
    StationLayout {
        id: stationLayout
        anchors.fill: parent
        dbManager: globalDatabaseManager
        
        // Database reset handling
        onDatabaseResetRequested: {
            globalDatabaseInitializer.resetDatabase()
        }
    }
    
    // === NOTIFICATION SYSTEM ===
    ToastNotification {
        id: toastNotification
        anchors.fill: parent
    }
}
```

### StationLayout.qml - Railway Visualization

**File** - `layouts/StationLayout.qml`

**Responsibilities**:
- Railway component visualization (signals, tracks, points)
- User interaction handling
- Real-time data refresh coordination
- Grid-based positioning system

#### Data Model Pattern

```cpp
Rectangle {
    id: stationLayout
    
    // === DATA MODELS (From Database) ===
    property var trackSegmentsModel: []
    property var outerSignalsModel: []
    property var homeSignalsModel: []
    property var starterSignalsModel: []
    property var advanceStarterSignalsModel: []
    property var pointMachinesModel: []
    property var textLabelsModel: []
    
    // === GRID SYSTEM ===
    property int cellSize: Math.floor(width / 320)  // Responsive grid
    property bool showGrid: true
    
    // === DATA REFRESH FUNCTIONS ===
    function refreshAllData() {
        if (!dbManager || !dbManager.isConnected) return
        
        refreshTrackSegmentData()
        refreshSignalData() 
        refreshPointMachineData()
        refreshTextLabelData()
    }
    
    function refreshSignalData() {
        var allSignals = dbManager.getAllSignalsList()
        
        // Filter by signal type
        outerSignalsModel = allSignals.filter(signal => signal.type === "OUTER")
        homeSignalsModel = allSignals.filter(signal => signal.type === "HOME")  
        starterSignalsModel = allSignals.filter(signal => signal.type === "STARTER")
        advanceStarterSignalsModel = allSignals.filter(signal => signal.type === "ADVANCED_STARTER")
    }
}
```

### Component Instantiation Pattern

```cpp
// Grid canvas for positioning
GridCanvas {
    id: canvas
    anchors.fill: parent
    gridSize: stationLayout.cellSize
    showGrid: stationLayout.showGrid
    
    // === TRACK SEGMENTS ===
    Repeater {
        model: trackSegmentsModel
        
        TrackSegment {
            x: modelData.startCol * stationLayout.cellSize
            y: modelData.startRow * stationLayout.cellSize
            width: (modelData.endCol - modelData.startCol) * stationLayout.cellSize
            height: (modelData.endRow - modelData.startRow) * stationLayout.cellSize
            
            segmentId: modelData.id
            segmentName: modelData.name
            isOccupied: modelData.isOccupied
            isAssigned: modelData.isAssigned
            isActive: modelData.isActive
            
            onSegmentClicked: stationLayout.handleTrackSegmentClick(segmentId)
        }
    }
    
    // === OUTER SIGNALS ===
    Repeater {
        model: outerSignalsModel
        
        OuterSignal {
            x: modelData.col * stationLayout.cellSize
            y: modelData.row * stationLayout.cellSize
            
            signalId: modelData.id
            signalName: modelData.name
            currentAspect: modelData.currentAspect
            aspectCount: modelData.aspectCount
            possibleAspects: modelData.possibleAspects || []
            direction: modelData.direction
            isActive: modelData.isActive
            
            cellSize: stationLayout.cellSize
            
            onSignalClicked: stationLayout.handleOuterSignalClick(signalId, currentAspect)
            onContextMenuRequested: signalContextMenu.show(x, y, signalId, signalName, currentAspect, possibleAspects)
        }
    }
}
```

### Signal Component Architecture

**Files**: `components/OuterSignal.qml`, `components/HomeSignal.qml`
Each signal component follows a consistent pattern:

```cpp
Rectangle {
    id: signalComponent
    
    // === PROPERTIES ===
    property string signalId: ""
    property string signalName: ""
    property string currentAspect: "RED"
    property int aspectCount: 2
    property var possibleAspects: []
    property string direction: "UP"
    property bool isActive: true
    property int cellSize: 20
    
    // === SIGNALS ===
    signal signalClicked(string signalId, string currentAspect)
    signal contextMenuRequested(string signalId, string signalName, string currentAspect, var possibleAspects, real x, real y)
    
    // === VISUAL STATE ===
    color: getAspectColor(currentAspect)
    opacity: isActive ? 1.0 : 0.5
    
    // === INTERACTION ===
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        
        onClicked: function(mouse) {
            if (mouse.button === Qt.LeftButton) {
                signalComponent.signalClicked(signalId, currentAspect)
            } else if (mouse.button === Qt.RightButton) {
                contextMenuRequested(signalId, signalName, currentAspect, possibleAspects, 
                                   mapToItem(parent, mouse.x, mouse.y).x,
                                   mapToItem(parent, mouse.x, mouse.y).y)
            }
        }
    }
    
    // === ASPECT COLOR MAPPING ===
    function getAspectColor(aspect) {
        switch(aspect) {
            case "RED": return "#e53e3e"
            case "YELLOW": return "#d69e2e" 
            case "GREEN": return "#38a169"
            case "DOUBLE_YELLOW": return "#f6ad55"
            default: return "#a0aec0"
        }
    }
}
```

### Safety & Interlocking System

#### Interlocking Rule Engine

**Files**: `interlocking/InterlockingRuleEngine.h`, `interlocking/InterlockingRuleEngine.cpp`
The rule engine processes JSON-based interlocking rules and validates signal operations:

```cpp
class InterlockingRuleEngine : public QObject {
    Q_OBJECT
    
public:
    // Rule evaluation
    ValidationResult validateInterlockedSignalAspectChange(
        const QString& signalId,
        const QString& currentAspect, 
        const QString& requestedAspect
    );
    
    // Rule loading
    bool loadRulesFromJson(const QString& filePath);
    bool loadRulesFromDatabase();

private:
    QJsonObject m_rules;                    // JSON rule definitions
    QHash<QString, SignalRule> m_signalRules; // Parsed rules cache
    
    // Rule processing
    QStringList findApplicableRules(const QString& signalId, const QString& aspect);
    ValidationResult evaluateRule(const SignalRule& rule, const QString& signalId);
};
```

### Signal Interlocking Rules Format

**File**: `resources/data/signal_interlocking_rules.json`,
```cpp
{
  "interlocking_rules": {
    "OS001": {
      "signal_name": "Outer Signal A1",
      "rules": [
        {
          "condition": "aspect_change_to_GREEN",
          "requirements": [
            {
              "type": "signal_aspect",
              "target": "HS001", 
              "required_aspect": "RED",
              "reason": "Home signal must be at danger"
            },
            {
              "type": "track_clear",
              "target": "T001",
              "reason": "Track section must be clear"
            }
          ]
        }
      ]
    }
  }
}
```

### Reactive Safety System

**File**: `interlocking/TrackCircuitBranch.h`, `interlocking/TrackCircuitBranch.cpp`

Handles automatic safety responses to track occupancy changes:

```cpp
class TrackCircuitBranch : public QObject {
    Q_OBJECT
    
public:
    // === MAIN REACTIVE FUNCTION ===
    void enforceTrackSegmentOccupancyInterlocking(
        const QString& trackSegmentId, 
        bool wasOccupied, 
        bool isOccupied
    );

signals:
    void systemFreezeRequired(const QString& trackSegmentId, const QString& reason, const QString& details);
    void automaticInterlockingCompleted(const QString& trackSegmentId, const QStringList& affectedSignals);
    void interlockingFailure(const QString& trackSegmentId, const QString& failedSignals, const QString& error);

private:
    struct TrackSegmentState {
        bool isOccupied;
        bool isAssigned; 
        bool isActive;
        QString occupiedBy;
        QString trackSegmentType;
        QStringList protectingSignals;
    };
    
    // Safety enforcement
    bool enforceSignalToRed(const QString& signalId, const QString& reason);
    bool enforceMultipleSignalsToRed(const QStringList& signalIds, const QString& reason);
    
    // Failure handling
    void handleInterlockingFailure(const QString& trackSegmentId, const QString& failedSignals, const QString& error);
    void emitSystemFreeze(const QString& trackSegmentId, const QString& reason, const QString& details);
};
```

### Track Occupancy Flow

```
Track Becomes Occupied
        ↓
DatabaseManager detects change (polling)
        ↓
TrackCircuitBranch::enforceTrackSegmentOccupancyInterlocking()
        ↓
Get protecting signals for track section
        ↓
Attempt to set all protecting signals to RED
        ↓
┌─── Success ────┐           ┌─── Failure ────┐
│ Emit success   │           │ Emit system    │
│ signal         │           │ freeze signal  │
└────────────────┘           └────────────────┘
        ↓                            ↓
Continue normal operation     Manual intervention required
```

## Data Flow & Signal Patterns
#### Qt Signal/Slot Connections

The application uses Qt's signal/slot mechanism for loose coupling:

```cpp
// Database → Interlocking Service
connect(dbManager, &DatabaseManager::connectionStateChanged,
        interlockingService, &InterlockingService::initialize);

// Interlocking Service → Main Application
connect(interlockingService, &InterlockingService::systemFreezeRequired,
        this, &MainApp::handleSystemFreeze);

// Database → QML (via properties)
connect(dbManager, &DatabaseManager::signalsChanged,
        qmlEngine, &QQmlEngine::signalsChangedNotification);
```

### QML-C++ Signal Flow

```cpp
// QML signal emission (user action)
OuterSignal {
    onSignalClicked: function(signalId, currentAspect) {
        // Call C++ method directly
        globalDatabaseManager.updateSignalAspect(signalId, "GREEN")
    }
}

// C++ signal reception (database update)
Connections {
    target: globalDatabaseManager
    
    function onSignalUpdated(signalId) {
        // Refresh specific signal
        refreshSignalData()
    }
    
    function onOperationBlocked(entityId, reason) {
        // Show user notification
        toastNotification.show("Operation Blocked", reason)
    }
}
```

**Data update patterns**:
- Polling Pattern (Periodic Updates)
```cpp
// DatabaseManager polling timer
QTimer* pollingTimer = new QTimer(this);
pollingTimer->setInterval(500); // 500ms for production
connect(pollingTimer, &QTimer::timeout, this, &DatabaseManager::refreshAllData);

void DatabaseManager::refreshAllData() {
    // Check for database changes
    if (hasDataChanged()) {
        emit signalsChanged();
        emit trackSegmentsChanged();
        emit pointMachinesChanged();
    }
}
```
- LISTEN/NOTIFY Pattern (Real-time)

```sql
-- Database triggers
CREATE OR REPLACE FUNCTION notify_railway_changes()
RETURNS TRIGGER AS $$
BEGIN
    PERFORM pg_notify('railway_changes', json_build_object(
        'table', TG_TABLE_NAME,
        'operation', TG_OP,
        'id', COALESCE(NEW.signal_id, NEW.machine_id, NEW.segment_id)
    )::text);
    RETURN COALESCE(NEW, OLD);
END;
$$ LANGUAGE plpgsql;

-- Apply to all operational tables
CREATE TRIGGER signals_notify AFTER INSERT OR UPDATE OR DELETE ON railway_control.signals
    FOR EACH ROW EXECUTE FUNCTION notify_railway_changes();
```

```cpp
// C++ LISTEN/NOTIFY handling
void DatabaseManager::enableRealTimeUpdates() {
    if (db.driver()->subscribeToNotification("railway_changes")) {
        connect(db.driver(), &QSqlDriver::notification,
                this, &DatabaseManager::handleDatabaseNotification);
    }
}

void DatabaseManager::handleDatabaseNotification(const QString& name, 
                                                QSqlDriver::NotificationSource source,
                                                const QVariant& payload) {
    QJsonDocument doc = QJsonDocument::fromJson(payload.toString().toUtf8());
    QJsonObject obj = doc.object();
    
    QString table = obj["table"].toString();
    QString operation = obj["operation"].toString();
    QString id = obj["id"].toString();
    
    if (table == "signals") {
        emit signalUpdated(id);
    } else if (table == "track_segments") {
        emit trackSegmentUpdated(id);
    }
}
```

### Performance & Real-time Considerations
**Performance Targets**:
- Interlocking Response: < 50ms for safety validations
- UI Updates: 30+ FPS for smooth interaction
- Database Polling: 400-500ms intervals for production
- Signal Operations: < 100ms end-to-end

### Performance Monitoring:
```cpp
class InterlockingService {
private:
    static constexpr double TARGET_RESPONSE_TIME_MS = 50.0;
    QList<double> m_responseHistory;
    
    void recordResponseTime(double responseTime) {
        m_responseHistory.append(responseTime);
        if (m_responseHistory.size() > 100) {
            m_responseHistory.removeFirst();
        }
        
        if (responseTime > TARGET_RESPONSE_TIME_MS) {
            logPerformanceWarning("Interlocking", responseTime);
        }
    }
    
    void logPerformanceWarning(const QString& operation, double responseTime) {
        qWarning() << "⚠️ PERFORMANCE:" << operation << "took" << responseTime 
                   << "ms (target:" << TARGET_RESPONSE_TIME_MS << "ms)";
    }
};
```

### Memory Management for 24/7 Operation
```cpp
// Bounded log storage
class EventLogger {
    std::deque<LogEntry> recentEvents;
    static constexpr size_t MAX_EVENTS = 10000;
    
public:
    void addEvent(const LogEntry& event) {
        recentEvents.push_back(event);
        if (recentEvents.size() > MAX_EVENTS) {
            recentEvents.pop_front();  // Remove oldest
        }
    }
};

// Smart pointer usage throughout
class InterlockingService {
private:
    std::unique_ptr<SignalBranch> m_signalBranch;
    std::unique_ptr<TrackCircuitBranch> m_trackSegmentBranch;
    std::unique_ptr<PointMachineBranch> m_pointBranch;
};
```

### Safety-Critical Features
#### System Freeze Mechanism
When critical safety violations occur, the system activates a freeze state:
```cpp
void InterlockingService::handleCriticalFailure(const QString& trackSegmentId, 
                                               const QString& reason,
                                               const QString& details) {
    // 1. Log critical event
    qCritical() << "🚨 CRITICAL SAFETY FAILURE:" << trackSegmentId << reason;
    
    // 2. Emit system freeze signal
    emit systemFreezeRequired(trackSegmentId, reason, details);
    
    // 3. Set operational state to false
    m_isOperational = false;
    emit operationalStateChanged(false);
    
    // 4. Block all future operations
    // (All validation methods check m_isOperational first)
}
```

### Audit Trail & Compliance
All safety-critical operations are logged:
```sql
-- Audit table structure
CREATE TABLE railway_audit.signal_operations (
    id SERIAL PRIMARY KEY,
    signal_id VARCHAR(20) NOT NULL,
    operation_type VARCHAR(50) NOT NULL,
    from_aspect VARCHAR(20),
    to_aspect VARCHAR(20),
    operator_id VARCHAR(100) NOT NULL,
    operation_timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    validation_result JSONB,
    response_time_ms NUMERIC(10,3),
    success BOOLEAN NOT NULL,
    failure_reason TEXT
);
```
### Input Validation & Sanitization
```cpp
bool SignalBranch::isValidAspectTransition(const QString& fromAspect, const QString& toAspect) {
    // Define valid transitions for railway safety
    static const QHash<QString, QStringList> validTransitions = {
        {"RED", {"YELLOW", "GREEN"}},          // Can proceed from danger
        {"YELLOW", {"RED", "GREEN"}},          // Can change to danger or clear
        {"GREEN", {"RED", "YELLOW"}},          // Can change to restrictive
        {"DOUBLE_YELLOW", {"RED", "YELLOW", "GREEN"}}  // 4-aspect signals
    };
    
    return validTransitions.value(fromAspect).contains(toAspect);
}

ValidationResult SignalBranch::validateInputs(const QString& signalId, const QString& aspect) {
    // Signal ID format validation
    if (!signalId.matches(QRegularExpression("^[A-Z]{2}\\d{3}$"))) {
        return ValidationResult::blocked("Invalid signal ID format", "INVALID_INPUT");
    }
    
    // Aspect validation
    static const QStringList validAspects = {"RED", "YELLOW", "GREEN", "DOUBLE_YELLOW"};
    if (!validAspects.contains(aspect)) {
        return ValidationResult::blocked("Invalid signal aspect", "INVALID_INPUT");
    }
    
    return ValidationResult::allowed();
}
```
## Build System & Development

### CMake Configuration

**File**: `CMakeLists.txt`

```cpp
cmake_minimum_required(VERSION 3.16)
project(RailFlux VERSION 0.1 LANGUAGES CXX)

# C++20 requirement for modern safety features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 dependencies
find_package(Qt6 REQUIRED COMPONENTS Quick Sql)
qt_standard_project_setup(REQUIRES 6.8)

# Executable target
qt_add_executable(appRailFlux main.cpp)

# QML module definition
qt_add_qml_module(appRailFlux
    URI RailFlux
    VERSION 1.0
    
    # QML interface files
    QML_FILES
        Main.qml
        layouts/StationLayout.qml
        components/TrackSegment.qml
        components/OuterSignal.qml
        components/HomeSignal.qml
        components/StarterSignal.qml
        components/AdvanceStarterSignal.qml
        components/PointMachine.qml
        components/GridCanvas.qml
        components/ToastNotification.qml
        components/SignalContextMenu.qml
    
    # C++ source files  
    SOURCES
        database/DatabaseManager.h database/DatabaseManager.cpp
        database/DatabaseInitializer.h database/DatabaseInitializer.cpp
        interlocking/InterlockingService.h interlocking/InterlockingService.cpp
        interlocking/SignalBranch.h interlocking/SignalBranch.cpp
        interlocking/TrackCircuitBranch.h interlocking/TrackCircuitBranch.cpp
        interlocking/PointMachineBranch.h interlocking/PointMachineBranch.cpp
        interlocking/SignalRule.h interlocking/SignalRule.cpp
        interlocking/InterlockingRuleEngine.h interlocking/InterlockingRuleEngine.cpp
    
    # Resource files
    RESOURCES
        sql/sql_coomands_railflux.sql
)

# Application resources
qt_add_resources(appRailFlux "app_resources"
    PREFIX "/"
    FILES
        resources/icons/railway-icon.ico
        resources/data/signal_interlocking_rules.json
)

# Link Qt libraries
target_link_libraries(appRailFlux PRIVATE Qt6::Quick Qt6::Sql)
```

## File Structure Summary

```
RailFlux/
├── main.cpp                              # Application entry point
├── CMakeLists.txt                        # Build configuration
├── Main.qml                              # QML application shell
├── database/
│   ├── DatabaseManager.{h,cpp}           # Core database interface
│   └── DatabaseInitializer.{h,cpp}       # Schema setup & population
├── interlocking/
│   ├── InterlockingService.{h,cpp}       # Safety validation coordinator
│   ├── SignalBranch.{h,cpp}              # Signal validation logic
│   ├── TrackCircuitBranch.{h,cpp}        # Track reactive safety
│   ├── PointMachineBranch.{h,cpp}        # Point machine validation
│   ├── InterlockingRuleEngine.{h,cpp}    # Rule evaluation engine
│   └── SignalRule.{h,cpp}                # Individual rule definitions
├── layouts/
│   └── StationLayout.qml                 # Main railway visualization
├── components/
│   ├── TrackSegment.qml                  # Track section visualization
│   ├── OuterSignal.qml                   # Outer signal component
│   ├── HomeSignal.qml                    # Home signal component
│   ├── StarterSignal.qml                 # Starter signal component
│   ├── AdvanceStarterSignal.qml          # Advanced starter signal
│   ├── PointMachine.qml                  # Point machine component
│   ├── GridCanvas.qml                    # Grid overlay
│   ├── ToastNotification.qml             # User notification system
│   └── SignalContextMenu.qml             # Right-click signal menu
├── sql/
│   └── sql_coomands_railflux.sql         # Complete database schema
├── resources/
│   ├── icons/railway-icon.ico            # Application icon
│   └── data/signal_interlocking_rules.json # JSON interlocking rules
└── CLAUDE.md                             # Development documentation
```