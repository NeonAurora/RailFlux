# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System & Development Commands

This is a Qt6/QML application using CMake as the build system.

### Build Commands
```bash
# Configure and build (from project root)
cmake -B build -S . -DCMAKE_PREFIX_PATH=/path/to/Qt6
cmake --build build

# Or using Qt Creator
# Open CMakeLists.txt in Qt Creator and build normally

# Executable location after build
./build/Desktop_Qt_6_9_1_MinGW_64_bit-Debug/appRailFlux.exe  # Windows
./build/appRailFlux  # Linux/macOS
```

### Dependencies
- Qt6 (minimum 6.8) with Quick and Sql modules
- PostgreSQL (supports both system and portable installations)
- C++20 compiler

## Application Architecture

RailFlux is a railway control system with a layered architecture:

```
┌─────────────────────────────────────────────────────────┐
│                    QML Interface                        │
├─────────────────────────────────────────────────────────┤
│                InterlockingService                      │
│  ┌─────────────┬──────────────┬─────────────────────┐   │
│  │SignalBranch │TrackBranch   │PointMachineBranch   │   │
│  └─────────────┴──────────────┴─────────────────────┘   │
├─────────────────────────────────────────────────────────┤
│                DatabaseManager                          │
│            (Direct Queries Only)                        │
├─────────────────────────────────────────────────────────┤
│              PostgreSQL Database                        │
│        (State Storage + Interlocking Rules)             │
└─────────────────────────────────────────────────────────┘
```

### Core Components

**QML Frontend** (`Main.qml`, `components/`, `layouts/`):
- Qt Quick/QML-based user interface
- Station layout visualization with interactive railway components
- Real-time updates via Qt signals/slots

**Database Layer** (`database/`):
- `DatabaseManager`: Main database interface with real-time polling and LISTEN/NOTIFY
- `DatabaseInitializer`: Database schema setup and initialization
- Supports both portable PostgreSQL and system installations

**Interlocking System** (`interlocking/`):
- `InterlockingService`: Central safety validation service
- `SignalBranch`: Signal operation validation and control
- `TrackCircuitBranch`: Track circuit occupancy management
- `PointMachineBranch`: Point machine position control
- `InterlockingRuleEngine`: Rule evaluation engine
- `SignalRule`: Individual interlocking rule definitions

**Key Design Patterns:**
- **Safety-First**: All operations go through interlocking validation
- **Reactive**: Hardware-driven track occupancy changes trigger automatic safety responses
- **Real-time**: LISTEN/NOTIFY for immediate database updates
- **Validation Result Pattern**: Structured responses with severity levels

### Data Flow

1. **Operator Actions**: QML → InterlockingService validation → DatabaseManager → PostgreSQL
2. **Hardware Events**: Track occupancy detected → DatabaseManager polling → InterlockingService reactive protection
3. **UI Updates**: Database NOTIFY → DatabaseManager signals → QML property updates

### Database Schema

The PostgreSQL database uses three schemas:
- `railway_control`: Main operational data (signals, tracks, points)
- `railway_audit`: Event logging and audit trail
- `railway_config`: Configuration lookup tables

Key tables include track_segments, track_circuits, signals, point_machines, and interlocking rules.

### Configuration Files

- `resources/data/signal_interlocking_rules.json`: JSON-based interlocking rules definition
- `sql/sql_coomands_railflux.sql`: Complete database schema and initialization
- `CMakeLists.txt`: Build configuration with Qt6 integration

### QML-C++ Integration

C++ classes are registered with QML in `main.cpp:14-17`:
- `DatabaseManager` as "RailFlux.Database" 
- `InterlockingService` as "RailFlux.Interlocking"
- Global instances available as `globalDatabaseManager`, `globalInterlockingService`

### Performance Considerations

- Target response time: 50ms for interlocking operations
- Configurable polling intervals (400ms-500ms for production)
- Performance monitoring with response time history tracking
- Automatic system freeze on critical safety violations

### Safety Features

- Critical safety violations trigger system freeze signals
- All signal/point operations validated through interlocking rules
- Track occupancy changes trigger automatic protective responses
- Comprehensive audit logging for regulatory compliance