#include "DatabaseInitializer.h"
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QThread>

DatabaseInitializer::DatabaseInitializer(QObject* parent)
    : QObject(parent)
    , resetTimer(new QTimer(this))
{
    resetTimer->setSingleShot(true);
    connect(resetTimer, &QTimer::timeout, this, &DatabaseInitializer::performReset);
}

DatabaseInitializer::~DatabaseInitializer() {
    if (db.isOpen()) {
        db.close();
    }
}

bool DatabaseInitializer::connectToDatabase() {
    if (db.isOpen()) {
        db.close();
    }

    // Try system PostgreSQL first
    if (connectToSystemPostgreSQL()) {
        qDebug() << "✅ DatabaseInitializer: Connected to system PostgreSQL";
        return true;
    }

    qDebug() << "🔄 DatabaseInitializer: System PostgreSQL unavailable, trying portable mode...";

    // Fall back to portable PostgreSQL
    if (connectToPortablePostgreSQL()) {
        qDebug() << "✅ DatabaseInitializer: Connected to portable PostgreSQL";
        return true;
    }

    setError("Failed to connect to any PostgreSQL instance");
    return false;
}

bool DatabaseInitializer::connectToSystemPostgreSQL() {
    try {
        if (QSqlDatabase::contains("initializer_system_connection")) {
            QSqlDatabase::removeDatabase("initializer_system_connection");
        }

        db = QSqlDatabase::addDatabase("QPSQL", "initializer_system_connection");
        db.setHostName("localhost");
        db.setPort(m_systemPort);
        db.setDatabaseName("railway_control_system");
        db.setUserName("postgres");
        db.setPassword("qwerty");

        if (db.open()) {
            qDebug() << "✅ DatabaseInitializer: Connected to system PostgreSQL";
            return true;
        }
    } catch (...) {
        qDebug() << "❌ DatabaseInitializer: System PostgreSQL connection failed";
    }

    if (db.isOpen()) {
        db.close();
    }
    return false;
}

bool DatabaseInitializer::connectToPortablePostgreSQL() {
    try {
        if (QSqlDatabase::contains("initializer_portable_connection")) {
            QSqlDatabase::removeDatabase("initializer_portable_connection");
        }

        db = QSqlDatabase::addDatabase("QPSQL", "initializer_portable_connection");
        db.setHostName("localhost");
        db.setPort(m_portablePort);
        db.setDatabaseName("railway_control_system");
        db.setUserName("postgres");
        db.setPassword("qwerty");

        if (db.open()) {
            qDebug() << "✅ DatabaseInitializer: Connected to portable PostgreSQL on port" << m_portablePort;
            return true;
        }
    } catch (...) {
        qDebug() << "❌ DatabaseInitializer: Portable PostgreSQL connection failed";
    }

    if (db.isOpen()) {
        db.close();
    }
    return false;
}

void DatabaseInitializer::resetDatabaseAsync() {
    if (m_isRunning) {
        qWarning() << "Database reset already in progress";
        return;
    }

    m_isRunning = true;
    emit isRunningChanged();

    updateProgress(0, "Preparing database reset...");
    resetTimer->start(100);
}

// ✅ UPDATED: Added populateTrackCircuits() call at step 45%
void DatabaseInitializer::performReset() {
    bool success = false;
    QString resultMessage;

    try {
        updateProgress(5, "Connecting to database...");
        if (!connectToDatabase()) {
            throw std::runtime_error("Failed to connect to database");
        }

        updateProgress(10, "Dropping existing schemas...");
        if (!dropExistingSchemas()) {
            throw std::runtime_error("Failed to drop existing schemas");
        }

        updateProgress(20, "Creating database schemas...");
        if (!createSchemas()) {
            throw std::runtime_error("Failed to create schemas");
        }

        updateProgress(40, "Populating configuration data...");
        if (!populateConfigurationData()) {
            throw std::runtime_error("Failed to populate configuration data");
        }

        // ✅ NEW: Populate trackSegment circuits BEFORE trackSegment segments
        updateProgress(45, "Populating track_segment circuits...");
        if (!populateTrackCircuits()) {
            throw std::runtime_error("Failed to populate track_segment circuits");
        }

        updateProgress(50, "Populating track_segment segments...");
        if (!populateTrackSegments()) {
            throw std::runtime_error("Failed to populate track_segment segments");
        }

        updateProgress(60, "Populating signals...");
        if (!populateSignals()) {
            throw std::runtime_error("Failed to populate signals");
        }

        updateProgress(80, "Populating point machines...");
        if (!populatePointMachines()) {
            throw std::runtime_error("Failed to populate point machines");
        }

        updateProgress(90, "Populating text labels...");
        if (!populateTextLabels()) {
            throw std::runtime_error("Failed to populate text labels");
        }

        updateProgress(88, "Populating interlocking rules...");
        if (!populateInterlockingRules()) {
            throw std::runtime_error("Failed to populate interlocking rules");
        }

        updateProgress(90, "Installing route assignment schema extensions...");
        if (!executeRouteAssignmentSchema()) {
            throw std::runtime_error("Failed to install route assignment schema");
        }

        updateProgress(93, "Populating route assignment data...");
        if (!populateRouteAssignmentData()) {
            throw std::runtime_error("Failed to populate route assignment data");
        }

        updateProgress(95, "Validating database...");
        if (!validateDatabase()) {
            throw std::runtime_error("Database validation failed");
        }

        updateProgress(100, "Database reset completed successfully!");
        success = true;
        resultMessage = "Database has been reset and populated with fresh data";

    } catch (const std::exception& e) {
        resultMessage = QString("Database reset failed: %1").arg(e.what());
        setError(resultMessage);
    }

    m_isRunning = false;
    emit isRunningChanged();
    emit resetCompleted(success, resultMessage);
}

bool DatabaseInitializer::dropExistingSchemas() {
    QStringList dropQueries = {
        "DROP SCHEMA IF EXISTS railway_control CASCADE;",
        "DROP SCHEMA IF EXISTS railway_audit CASCADE;",
        "DROP SCHEMA IF EXISTS railway_config CASCADE;",
        "DROP SEQUENCE IF EXISTS railway_audit.event_sequence CASCADE;",
        "DROP ROLE IF EXISTS railway_operator;",
        "DROP ROLE IF EXISTS railway_observer;",
        "DROP ROLE IF EXISTS railway_auditor;"
    };

    for (const QString& query : dropQueries) {
        if (!executeQuery(query)) {
            return false;
        }
    }

    return true;
}

bool DatabaseInitializer::createSchemas() {
    if (!executeSchemaScript()) {
        return false;
    }
    return verifySchemas();
}

bool DatabaseInitializer::verifySchemas() {
    QStringList requiredSchemas = {"railway_control", "railway_audit", "railway_config"};

    for (const QString& schemaName : requiredSchemas) {
        QSqlQuery query(db);
        query.prepare("SELECT COUNT(*) FROM information_schema.schemata WHERE schema_name = ?");
        query.addBindValue(schemaName);

        if (!query.exec() || !query.next() || query.value(0).toInt() == 0) {
            setError(QString("Schema %1 does not exist").arg(schemaName));
            return false;
        }
    }

    qDebug() << "All required schemas verified successfully";
    return true;
}

void DatabaseInitializer::testConnectionAsync() {
    bool success = false;
    QString message;

    try {
        if (connectToDatabase()) {
            QSqlQuery query(db);
            if (query.exec("SELECT version()") && query.next()) {
                QString version = query.value(0).toString();
                success = true;
                message = QString("Connection successful!\nPostgreSQL version: %1").arg(version);
            } else {
                message = "Connected but failed to query version";
            }
        } else {
            message = "Failed to connect to any PostgreSQL instance";
        }
    } catch (...) {
        message = "Connection test failed with exception";
    }

    emit connectionTestCompleted(success, message);
}

bool DatabaseInitializer::executeSchemaScript() {
    // Step 1: Create schemas
    QStringList schemaCreationQueries = {
        "CREATE SCHEMA IF NOT EXISTS railway_control;",
        "CREATE SCHEMA IF NOT EXISTS railway_audit;",
        "CREATE SCHEMA IF NOT EXISTS railway_config;",
        "COMMENT ON SCHEMA railway_control IS 'Main railway control system operational data';",
        "COMMENT ON SCHEMA railway_audit IS 'Audit trail and event logging for compliance';",
        "COMMENT ON SCHEMA railway_config IS 'Configuration and lookup tables';",
    };

    qDebug() << "Creating schemas...";
    for (const QString& query : schemaCreationQueries) {
        if (!executeQuery(query.trimmed())) {
            setError(QString("Failed to create schema: %1").arg(query));
            return false;
        }
    }

    // Step 2: Set search path
    if (!executeQuery("SET search_path TO railway_control, railway_audit, railway_config, public;")) {
        setError("Failed to set search path");
        return false;
    }

    // Step 3: Create configuration tables
    QStringList configTables = {
        R"(CREATE TABLE railway_config.signal_types (
            id SERIAL PRIMARY KEY,
            type_code VARCHAR(20) NOT NULL UNIQUE,
            type_name VARCHAR(50) NOT NULL,
            description TEXT,
            max_aspects INTEGER NOT NULL DEFAULT 2,
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
        ))",

        R"(CREATE TABLE railway_config.signal_aspects (
            id SERIAL PRIMARY KEY,
            aspect_code VARCHAR(20) NOT NULL UNIQUE,
            aspect_name VARCHAR(50) NOT NULL,
            color_code VARCHAR(7) NOT NULL,
            description TEXT,
            safety_level INTEGER NOT NULL DEFAULT 0,
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
        ))",

        R"(CREATE TABLE railway_config.point_positions (
            id SERIAL PRIMARY KEY,
            position_code VARCHAR(20) NOT NULL UNIQUE,
            position_name VARCHAR(50) NOT NULL,
            description TEXT,
            created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
        ))"
    };

    qDebug() << "Creating configuration tables...";
    for (const QString& query : configTables) {
        if (!executeQuery(query)) {
            setError(QString("Failed to create config table: %1").arg(query.left(50)));
            return false;
        }
    }

    // Step 4: Create main tables (IMPORTANT: track_circuits BEFORE track_segments)
    QStringList mainTables = {
        // ✅ FIRST: Create track_circuits table
        R"(CREATE TABLE railway_control.track_circuits (
        id SERIAL PRIMARY KEY,
        circuit_id VARCHAR(20) NOT NULL UNIQUE,
        circuit_name VARCHAR(100),
        is_occupied BOOLEAN DEFAULT FALSE,
        occupied_by VARCHAR(50),
        length_meters NUMERIC(10,2),
        max_speed_kmh INTEGER,
        is_active BOOLEAN DEFAULT TRUE,
        protecting_signals TEXT[],
        created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
        ))",

        // ✅ SECOND: Create track_segments table (references track_circuits)
        R"(CREATE TABLE railway_control.track_segments (
        id SERIAL PRIMARY KEY,
        segment_id VARCHAR(20) NOT NULL UNIQUE,
        segment_name VARCHAR(100),
        start_row NUMERIC(10,2) NOT NULL,
        start_col NUMERIC(10,2) NOT NULL,
        end_row NUMERIC(10,2) NOT NULL,
        end_col NUMERIC(10,2) NOT NULL,
        track_segment_type VARCHAR(20) DEFAULT 'STRAIGHT',
        is_assigned BOOLEAN DEFAULT FALSE,
        circuit_id VARCHAR(20) REFERENCES railway_control.track_circuits(circuit_id),
        length_meters NUMERIC(10,2),
        max_speed_kmh INTEGER,
        is_active BOOLEAN DEFAULT TRUE,
        protecting_signals TEXT[],
        created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
        CONSTRAINT chk_coordinates CHECK (
            start_row >= 0 AND start_col >= 0 AND
            end_row >= 0 AND end_col >= 0
        )
        ))",

        R"(CREATE TABLE railway_control.signals (
        id SERIAL PRIMARY KEY,
        signal_id VARCHAR(20) NOT NULL UNIQUE,
        signal_name VARCHAR(100) NOT NULL,
        signal_type_id INTEGER NOT NULL REFERENCES railway_config.signal_types(id),
        location_row NUMERIC(10,2) NOT NULL,
        location_col NUMERIC(10,2) NOT NULL,
        direction VARCHAR(10) NOT NULL CHECK (direction IN ('UP', 'DOWN')),

        -- ✅ MAIN SIGNAL ASPECT (unchanged)
        current_aspect_id INTEGER REFERENCES railway_config.signal_aspects(id),

        -- ✅ SUBSIDIARY SIGNAL ASPECTS (now using aspect IDs instead of VARCHAR)
        calling_on_aspect_id INTEGER REFERENCES railway_config.signal_aspects(id),
        loop_aspect_id INTEGER REFERENCES railway_config.signal_aspects(id),

        -- ✅ SIGNAL CONFIGURATION (unchanged)
        loop_signal_configuration VARCHAR(10) DEFAULT 'UR',
        aspect_count INTEGER NOT NULL DEFAULT 2,
        possible_aspects TEXT[],
        is_active BOOLEAN DEFAULT TRUE,
        location_description VARCHAR(200),

        -- ✅ AUDIT FIELDS (unchanged)
        last_changed_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
        last_changed_by VARCHAR(100),

        -- ✅ INTERLOCKING FIELDS (unchanged)
        interlocked_with INTEGER[],
        protected_track_circuits TEXT[],
        manual_control_active BOOLEAN DEFAULT FALSE,

        -- ✅ TIMESTAMP FIELDS (unchanged)
        created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,

        -- ✅ CONSTRAINTS (unchanged)
        CONSTRAINT chk_location CHECK (location_row >= 0 AND location_col >= 0),
        CONSTRAINT chk_aspect_count CHECK (aspect_count >= 2 AND aspect_count <= 4)
        ))",

        // ✅ FIXED: Added missing comma between constraints
        R"(CREATE TABLE railway_control.point_machines (
        id SERIAL PRIMARY KEY,
        machine_id VARCHAR(20) NOT NULL UNIQUE,
        machine_name VARCHAR(100) NOT NULL,
        junction_row NUMERIC(10,2) NOT NULL,
        junction_col NUMERIC(10,2) NOT NULL,
        root_track_segment_connection JSONB NOT NULL,
        normal_track_segment_connection JSONB NOT NULL,
        reverse_track_segment_connection JSONB NOT NULL,
        current_position_id INTEGER REFERENCES railway_config.point_positions(id),
        operating_status VARCHAR(20) DEFAULT 'CONNECTED' CHECK (
            operating_status IN ('CONNECTED', 'IN_TRANSITION', 'FAILED', 'LOCKED_OUT')
        ),
        transition_time_ms INTEGER DEFAULT 3000,
        last_operated_at TIMESTAMP WITH TIME ZONE,
        last_operated_by VARCHAR(100),
        operation_count INTEGER DEFAULT 0,
        safety_interlocks INTEGER[],
        paired_entity VARCHAR(20),
        is_locked BOOLEAN DEFAULT FALSE,
        lock_reason TEXT,
        protected_signals TEXT[],
        created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
        CONSTRAINT chk_junction_location CHECK (junction_row >= 0 AND junction_col >= 0),
        CONSTRAINT chk_no_self_pairing CHECK (machine_id != paired_entity)
        ))",

        R"(CREATE TABLE railway_control.text_labels (
        id SERIAL PRIMARY KEY,
        label_text VARCHAR(200) NOT NULL,
        position_row NUMERIC(10,2) NOT NULL,
        position_col NUMERIC(10,2) NOT NULL,
        font_size INTEGER DEFAULT 12,
        color VARCHAR(7) DEFAULT '#ffffff',
        font_family VARCHAR(50) DEFAULT 'Arial',
        is_visible BOOLEAN DEFAULT TRUE,
        label_type VARCHAR(20) DEFAULT 'INFO',
        created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
        ))",

        R"(CREATE TABLE railway_control.system_state (
        id SERIAL PRIMARY KEY,
        state_key VARCHAR(100) NOT NULL UNIQUE,
        state_value JSONB NOT NULL,
        description TEXT,
        last_updated TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
        updated_by VARCHAR(100)
        ))",

        R"(CREATE TABLE railway_control.interlocking_rules (
        id SERIAL PRIMARY KEY,
        rule_name VARCHAR(100) NOT NULL,
        source_entity_type VARCHAR(20) NOT NULL CHECK (source_entity_type IN ('SIGNAL', 'POINT_MACHINE', 'TRACK_SEGMENT', 'TRACK_CIRCUIT')),
        source_entity_id VARCHAR(20) NOT NULL,
        target_entity_type VARCHAR(20) NOT NULL CHECK (target_entity_type IN ('SIGNAL', 'POINT_MACHINE', 'TRACK_SEGMENT', 'TRACK_CIRCUIT')),
        target_entity_id VARCHAR(20) NOT NULL,
        target_constraint VARCHAR(50) NOT NULL,
        rule_type VARCHAR(50) NOT NULL,
        priority INTEGER DEFAULT 100,
        is_active BOOLEAN DEFAULT TRUE,
        created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
        CONSTRAINT chk_no_self_reference CHECK (
            NOT (source_entity_type = target_entity_type AND source_entity_id = target_entity_id)
        )
        ))"
    };

    qDebug() << "Creating main tables...";
    for (const QString& query : mainTables) {
        if (!executeQuery(query)) {
            setError(QString("Failed to create main table: %1").arg(query.left(50)));
            return false;
        }
    }

    // Step 5: Create audit tables
    QStringList auditTables = {
        R"(CREATE TABLE railway_audit.event_log (
            id BIGSERIAL PRIMARY KEY,
            event_timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            event_type VARCHAR(50) NOT NULL,
            entity_type VARCHAR(50) NOT NULL,
            entity_id VARCHAR(50) NOT NULL,
            entity_name VARCHAR(100),
            old_values JSONB,
            new_values JSONB,
            field_changed VARCHAR(100),
            operator_id VARCHAR(100),
            operator_name VARCHAR(200),
            operation_source VARCHAR(50) DEFAULT 'HMI',
            session_id VARCHAR(100),
            ip_address INET,
            safety_critical BOOLEAN DEFAULT FALSE,
            authorization_level VARCHAR(20),
            reason_code VARCHAR(50),
            comments TEXT,
            replay_data JSONB,
            sequence_number BIGINT,
            event_date DATE
        ))",

        R"(CREATE TABLE railway_audit.system_events (
            id BIGSERIAL PRIMARY KEY,
            event_timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
            event_level VARCHAR(20) NOT NULL CHECK (event_level IN ('INFO', 'WARNING', 'ERROR', 'CRITICAL')),
            event_category VARCHAR(50) NOT NULL,
            event_message TEXT NOT NULL,
            event_details JSONB,
            source_component VARCHAR(100),
            error_code VARCHAR(20),
            resolved_at TIMESTAMP WITH TIME ZONE,
            resolved_by VARCHAR(100)
        ))"
    };

    qDebug() << "Creating audit tables...";
    for (const QString& query : auditTables) {
        if (!executeQuery(query)) {
            setError(QString("Failed to create audit table: %1").arg(query.left(50)));
            return false;
        }
    }

    // Step 6: Create sequences
    QStringList sequences = {
        "CREATE SEQUENCE railway_audit.event_sequence"
    };

    qDebug() << "Creating sequences...";
    for (const QString& query : sequences) {
        if (!executeQuery(query)) {
            qWarning() << "Failed to create sequence:" << query;
        }
    }

    // Step 7: Create essential functions
    QStringList essentialFunctions = {
        R"(CREATE OR REPLACE FUNCTION railway_audit.set_event_date()
        RETURNS TRIGGER AS $$
        BEGIN
            NEW.event_date := NEW.event_timestamp::DATE;
            RETURN NEW;
        END;
        $$ LANGUAGE plpgsql)",

        R"(CREATE OR REPLACE FUNCTION railway_control.update_timestamp()
        RETURNS TRIGGER AS $$
        BEGIN
            NEW.updated_at = CURRENT_TIMESTAMP;
            RETURN NEW;
        END;
        $$ LANGUAGE plpgsql)",

        R"(CREATE OR REPLACE FUNCTION railway_control.update_signal_change_time()
        RETURNS TRIGGER AS $$
        BEGIN
            IF OLD.current_aspect_id IS DISTINCT FROM NEW.current_aspect_id THEN
                NEW.last_changed_at = CURRENT_TIMESTAMP;
            END IF;
            RETURN NEW;
        END;
        $$ LANGUAGE plpgsql)",

        R"(CREATE OR REPLACE FUNCTION railway_config.get_aspect_id(aspect_code_param VARCHAR)
        RETURNS INTEGER AS $$
        DECLARE
            aspect_id_result INTEGER;
        BEGIN
            SELECT id INTO aspect_id_result
            FROM railway_config.signal_aspects
            WHERE aspect_code = aspect_code_param;
            RETURN aspect_id_result;
        END;
        $$ LANGUAGE plpgsql)",

        R"(CREATE OR REPLACE FUNCTION railway_config.get_position_id(position_code_param VARCHAR)
        RETURNS INTEGER AS $$
        DECLARE
            position_id_result INTEGER;
        BEGIN
            SELECT id INTO position_id_result
            FROM railway_config.point_positions
            WHERE position_code = position_code_param;
            RETURN position_id_result;
        END;
        $$ LANGUAGE plpgsql)"
    };

    qDebug() << "Creating essential functions...";
    for (const QString& query : essentialFunctions) {
        if (!executeQuery(query)) {
            qWarning() << "Failed to create function:" << query.left(100) + "...";
        }
    }

    // Step 8: Create essential triggers
    QStringList essentialTriggers = {
        R"(CREATE TRIGGER trg_event_log_set_date
            BEFORE INSERT OR UPDATE ON railway_audit.event_log
            FOR EACH ROW EXECUTE FUNCTION railway_audit.set_event_date())",

        R"(CREATE TRIGGER trg_track_segments_updated_at
            BEFORE UPDATE ON railway_control.track_segments
            FOR EACH ROW EXECUTE FUNCTION railway_control.update_timestamp())",

        R"(CREATE TRIGGER trg_track_circuits_updated_at
            BEFORE UPDATE ON railway_control.track_circuits
            FOR EACH ROW EXECUTE FUNCTION railway_control.update_timestamp())",

        R"(CREATE TRIGGER trg_signals_updated_at
            BEFORE UPDATE ON railway_control.signals
            FOR EACH ROW EXECUTE FUNCTION railway_control.update_timestamp())",

        R"(CREATE TRIGGER trg_point_machines_updated_at
            BEFORE UPDATE ON railway_control.point_machines
            FOR EACH ROW EXECUTE FUNCTION railway_control.update_timestamp())",

        R"(CREATE TRIGGER trg_signals_aspect_changed
            BEFORE UPDATE ON railway_control.signals
            FOR EACH ROW EXECUTE FUNCTION railway_control.update_signal_change_time())",

        R"(CREATE TRIGGER trg_text_labels_updated_at
            BEFORE UPDATE ON railway_control.text_labels
            FOR EACH ROW EXECUTE FUNCTION railway_control.update_timestamp())"
    };

    qDebug() << "Creating essential triggers...";
    for (const QString& query : essentialTriggers) {
        if (!executeQuery(query)) {
            qWarning() << "Failed to create trigger:" << query.left(100) + "...";
        }
    }

    // Step 9: Create basic indexes (UPDATED: removed broken is_occupied index)
    QStringList basicIndexes = {
        "CREATE INDEX idx_track_segments_id ON railway_control.track_segments(segment_id)",
        "CREATE INDEX idx_track_segments_circuit ON railway_control.track_segments(circuit_id)",
        "CREATE INDEX idx_track_segments_assigned ON railway_control.track_segments(is_assigned) WHERE is_assigned = TRUE",
        "CREATE INDEX idx_track_segments_location ON railway_control.track_segments USING btree(start_row, start_col, end_row, end_col)",

        // ✅ NEW: Track Segment circuits indexes
        "CREATE INDEX idx_track_circuits_id ON railway_control.track_circuits(circuit_id)",
        "CREATE INDEX idx_track_circuits_occupied ON railway_control.track_circuits(is_occupied) WHERE is_occupied = TRUE",
        "CREATE INDEX idx_track_circuits_active ON railway_control.track_circuits(is_active) WHERE is_active = TRUE",

        "CREATE INDEX idx_signals_id ON railway_control.signals(signal_id)",
        "CREATE INDEX idx_signals_type ON railway_control.signals(signal_type_id)",
        "CREATE INDEX idx_signals_location ON railway_control.signals USING btree(location_row, location_col)",
        "CREATE INDEX idx_signals_active ON railway_control.signals(is_active) WHERE is_active = TRUE",
        "CREATE INDEX idx_signals_last_changed ON railway_control.signals(last_changed_at)",

        "CREATE INDEX idx_point_machines_id ON railway_control.point_machines(machine_id)",
        "CREATE INDEX idx_point_machines_position ON railway_control.point_machines(current_position_id)",
        "CREATE INDEX idx_point_machines_status ON railway_control.point_machines(operating_status)",
        "CREATE INDEX idx_point_machines_junction ON railway_control.point_machines USING btree(junction_row, junction_col)",
        "CREATE INDEX idx_point_machines_paired_entity ON railway_control.point_machines(paired_entity) WHERE paired_entity IS NOT NULL",

        "CREATE INDEX idx_event_log_timestamp ON railway_audit.event_log(event_timestamp)",
        "CREATE INDEX idx_event_log_entity ON railway_audit.event_log(entity_type, entity_id)",
        "CREATE INDEX idx_event_log_operator ON railway_audit.event_log(operator_id)",
        "CREATE INDEX idx_event_log_safety ON railway_audit.event_log(safety_critical) WHERE safety_critical = TRUE",
        "CREATE INDEX idx_event_log_sequence ON railway_audit.event_log(sequence_number)",
        "CREATE INDEX idx_event_log_date ON railway_audit.event_log(event_date)"
    };

    qDebug() << "Creating basic indexes...";
    for (const QString& query : basicIndexes) {
        if (!executeQuery(query)) {
            qWarning() << "Failed to create index:" << query.left(80) + "...";
        }
    }

    // Step 10: Create roles
    QStringList roles = {
        "CREATE ROLE railway_operator",
        "CREATE ROLE railway_observer",
        "CREATE ROLE railway_auditor"
    };

    qDebug() << "Creating roles...";
    for (const QString& query : roles) {
        executeQuery(query); // Ignore errors for roles
    }

    // Continue with advanced functions, triggers, etc.
    if (!createAdvancedFunctions()) {
        qWarning() << "Failed to create some advanced functions, continuing...";
    }

    if (!createAdvancedTriggers()) {
        qWarning() << "Failed to create some advanced triggers, continuing...";
    }

    if (!createGinIndexes()) {
        qWarning() << "Failed to create some GIN indexes, continuing...";
    }

    if (!createViews()) {
        qWarning() << "Failed to create some views, continuing...";
    }

    if (!setupRolePermissions()) {
        qWarning() << "Failed to set up some role permissions, continuing...";
    }

    qDebug() << "Complete schema creation finished successfully";
    return true;
}

bool DatabaseInitializer::populateConfigurationData() {
    // Insert signal types
    int starterTypeId = insertSignalType("STARTER", "Starter Signal", 3);
    int homeTypeId = insertSignalType("HOME", "Home Signal", 3);
    int outerTypeId = insertSignalType("OUTER", "Outer Signal", 4);
    int advancedStarterTypeId = insertSignalType("ADVANCED_STARTER", "Advanced Starter Signal", 2);

    if (starterTypeId <= 0 || homeTypeId <= 0 || outerTypeId <= 0 || advancedStarterTypeId <= 0) {
        return false;
    }

    // Insert signal aspects
    insertSignalAspect("RED", "Danger", "#e53e3e", 0);
    insertSignalAspect("YELLOW", "Caution", "#d69e2e", 1);
    insertSignalAspect("GREEN", "Clear", "#38a169", 2);
    insertSignalAspect("SINGLE_YELLOW", "Single Yellow", "#d69e2e", 1);
    insertSignalAspect("DOUBLE_YELLOW", "Double Yellow", "#f6ad55", 1);
    insertSignalAspect("WHITE", "Calling On", "#ffffff", 0);
    insertSignalAspect("BLUE", "Shunt", "#3182ce", 0);
    insertSignalAspect("OFF", "Inactive", "#cccccc", 0);

    // Insert point positions
    insertPointPosition("NORMAL", "Normal Position");
    insertPointPosition("REVERSE", "Reverse Position");

    return true;
}

bool DatabaseInitializer::populateTrackCircuits() {
    QJsonArray circuitData = getTrackCircuitMappings();

    QString insertQuery = R"(
        INSERT INTO railway_control.track_circuits
        (circuit_id, circuit_name, is_occupied, is_active, protecting_signals)
        VALUES (?, ?, FALSE, TRUE, ?)
        ON CONFLICT (circuit_id) DO NOTHING
    )";

    for (const auto& value : circuitData) {
        QJsonObject circuit = value.toObject();

        // ✅ CONVERT: JSON array to PostgreSQL TEXT[] format
        QJsonArray protectingSignalsArray = circuit["protecting_signals"].toArray();
        QStringList protectingSignalsList;
        for (const auto& signal : protectingSignalsArray) {
            protectingSignalsList.append(signal.toString());
        }

        // ✅ FORMAT: Create PostgreSQL array string {signal1,signal2,signal3}
        QString protectingSignalsStr;
        if (protectingSignalsList.isEmpty()) {
            protectingSignalsStr = "{}";  // Empty array
        } else {
            protectingSignalsStr = "{" + protectingSignalsList.join(",") + "}";
        }

        QVariantList params = {
            circuit["circuit_id"].toString(),
            circuit["circuit_name"].toString(),
            protectingSignalsStr
        };

        if (!executeQuery(insertQuery, params)) {
            return false;
        }
    }

    return true;
}


// ✅ UPDATED: Populate track segments WITH protecting signals
bool DatabaseInitializer::populateTrackSegments() {
    QJsonArray trackSegmentData = getTrackSegmentsData();

    QString insertQuery = R"(
        INSERT INTO railway_control.track_segments
        (segment_id, start_row, start_col, end_row, end_col, circuit_id, is_assigned, protecting_signals)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT (segment_id) DO NOTHING
    )";

    for (const auto& trackSegmentValue : trackSegmentData) {
        QJsonObject trackSegment = trackSegmentValue.toObject();

        // ✅ HANDLE: INVALID circuit_id by setting to NULL
        QString circuitId = trackSegment["circuit_id"].toString();
        QVariant circuitIdValue = (circuitId == "INVALID") ? QVariant() : QVariant(circuitId);

        // ✅ CONVERT: JSON array to PostgreSQL TEXT[] format
        QJsonArray protectingSignalsArray = trackSegment["protecting_signals"].toArray();
        QStringList protectingSignalsList;
        for (const auto& signal : protectingSignalsArray) {
            protectingSignalsList.append(signal.toString());
        }

        // ✅ FORMAT: Create PostgreSQL array string {signal1,signal2,signal3}
        QString protectingSignalsStr;
        if (protectingSignalsList.isEmpty()) {
            protectingSignalsStr = "{}";  // Empty array
        } else {
            protectingSignalsStr = "{" + protectingSignalsList.join(",") + "}";
        }

        QVariantList params = {
            trackSegment["id"].toString(),
            trackSegment["startRow"].toDouble(),
            trackSegment["startCol"].toDouble(),
            trackSegment["endRow"].toDouble(),
            trackSegment["endCol"].toDouble(),
            circuitIdValue,  // ✅ NULL for INVALID circuits
            trackSegment["assigned"].toBool(),
            protectingSignalsStr
        };

        if (!executeQuery(insertQuery, params)) {
            return false;
        }
    }

    return true;
}

int DatabaseInitializer::getAspectIdByCode(const QString& aspectCode) {
    // ✅ HARDCODED: Based on your specification
    if (aspectCode == "OFF") return 8;
    if (aspectCode == "YELLOW") return 2;
    if (aspectCode == "WHITE") return 6;

    // ✅ FALLBACK: Query database for other aspects
    QSqlQuery query(db);
    query.prepare("SELECT id FROM railway_config.signal_aspects WHERE aspect_code = ?");
    query.addBindValue(aspectCode);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    // ✅ DEFAULT: Return OFF aspect ID if not found
    qWarning() << "⚠️ Aspect code not found:" << aspectCode << "- defaulting to OFF";
    return 8; // OFF
}

// Rest of the methods remain the same...
bool DatabaseInitializer::populateSignals() {
    updateProgress(40, "Populating signals...");

    // Combine all signal types
    QJsonArray allSignals;
    QJsonArray outerSignals = getOuterSignalsData();
    QJsonArray homeSignals = getHomeSignalsData();
    QJsonArray starterSignals = getStarterSignalsData();
    QJsonArray advancedSignals = getAdvancedStarterSignalsData();

    // Merge all signal arrays
    for (const auto& signal : outerSignals) allSignals.append(signal);
    for (const auto& signal : homeSignals) allSignals.append(signal);
    for (const auto& signal : starterSignals) allSignals.append(signal);
    for (const auto& signal : advancedSignals) allSignals.append(signal);

    for (const auto& signalValue : allSignals) {
        QJsonObject signal = signalValue.toObject();
        QString signalType = signal["type"].toString();

        // ✅ Get signal type ID
        QSqlQuery typeQuery(db);
        typeQuery.prepare("SELECT id FROM railway_config.signal_types WHERE type_code = ?");
        typeQuery.addBindValue(signalType);

        if (!typeQuery.exec() || !typeQuery.next()) {
            setError(QString("Signal type not found: %1").arg(signalType));
            return false;
        }
        int typeId = typeQuery.value(0).toInt();

        // ✅ Get main signal aspect ID
        QString currentAspect = signal["currentAspect"].toString();
        QSqlQuery aspectQuery(db);
        aspectQuery.prepare("SELECT id FROM railway_config.signal_aspects WHERE aspect_code = ?");
        aspectQuery.addBindValue(currentAspect);

        int aspectId = 1; // Default to RED (assuming RED has id=1)
        if (aspectQuery.exec() && aspectQuery.next()) {
            aspectId = aspectQuery.value(0).toInt();
        }

        // ✅ Get calling-on aspect ID
        QString callingOnAspectStr = signal["callingOnAspect"].toString("OFF");
        int callingOnAspectId = getAspectIdByCode(callingOnAspectStr);

        // ✅ Get loop aspect ID
        QString loopAspectStr = signal["loopAspect"].toString("OFF");
        int loopAspectId = getAspectIdByCode(loopAspectStr);

        // ✅ Convert possible aspects array to PostgreSQL array format
        QJsonArray possibleAspects = signal["possibleAspects"].toArray();
        QStringList aspectsList;
        for (const auto& aspect : possibleAspects) {
            aspectsList << aspect.toString();
        }
        QString aspectsArrayStr = "{" + aspectsList.join(",") + "}";

        // ✅ NEW: Convert protected track circuits array to PostgreSQL TEXT[]
        QJsonArray protectedCircuitsArray = signal["protectedTrackCircuits"].toArray();
        QStringList protectedCircuitsList;
        for (const auto& circuit : protectedCircuitsArray) {
            protectedCircuitsList.append(circuit.toString());
        }

        QString protectedCircuitsStr;
        if (protectedCircuitsList.isEmpty()) {
            protectedCircuitsStr = "{}";
        } else {
            protectedCircuitsStr = "{" + protectedCircuitsList.join(",") + "}";
        }

        // ✅ UPDATED: Insert query with protected_track_circuits
        QString insertQuery = R"(
            INSERT INTO railway_control.signals
            (signal_id, signal_name, signal_type_id, location_row, location_col,
             direction, current_aspect_id, calling_on_aspect_id, loop_aspect_id,
             loop_signal_configuration, aspect_count, possible_aspects,
             protected_track_circuits, is_active, location_description)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT (signal_id) DO NOTHING
        )";

        // ✅ UPDATED: Parameters with new protected_track_circuits field
        QVariantList params = {
            signal["id"].toString(),
            signal["name"].toString(),
            typeId,
            signal["row"].toDouble(),
            signal["col"].toDouble(),
            signal["direction"].toString(),
            aspectId,                                                    // current_aspect_id
            callingOnAspectId,                                          // calling_on_aspect_id
            loopAspectId,                                               // loop_aspect_id
            signal["loopSignalConfiguration"].toString("UR"),
            signal["aspectCount"].toInt(2),
            aspectsArrayStr,
            protectedCircuitsStr,                                       // ✅ NEW: protected_track_circuits
            signal["isActive"].toBool(true),
            signal["location"].toString()
        };

        if (!executeQuery(insertQuery, params)) {
            return false;
        }
    }

    return true;
}

bool DatabaseInitializer::populatePointMachines() {
    QJsonArray pointsData = getPointMachinesData();

    for (const auto& pointValue : pointsData) {
        QJsonObject point = pointValue.toObject();

        // Get position ID
        QString currentPosition = point["position"].toString();
        QSqlQuery positionQuery(db);
        positionQuery.prepare("SELECT id FROM railway_config.point_positions WHERE position_code = ?");
        positionQuery.addBindValue(currentPosition);

        int positionId = 1; // Default to NORMAL
        if (positionQuery.exec() && positionQuery.next()) {
            positionId = positionQuery.value(0).toInt();
        }

        // Convert trackSegment connections to properly formatted JSON strings
        QJsonObject rootTrackSegment = point["rootTrackSegment"].toObject();
        QJsonObject normalTrackSegment = point["normalTrackSegment"].toObject();
        QJsonObject reverseTrackSegment = point["reverseTrackSegment"].toObject();

        QString rootTrackSegmentJson = QString::fromUtf8(QJsonDocument(rootTrackSegment).toJson(QJsonDocument::Compact));
        QString normalTrackSegmentJson = QString::fromUtf8(QJsonDocument(normalTrackSegment).toJson(QJsonDocument::Compact));
        QString reverseTrackSegmentJson = QString::fromUtf8(QJsonDocument(reverseTrackSegment).toJson(QJsonDocument::Compact));

        // NEW: Handle paired entity (can be null)
        QString pairedEntity;
        if (point.contains("pairedEntity") && !point["pairedEntity"].toString().isEmpty()) {
            pairedEntity = point["pairedEntity"].toString();
        }

        // UPDATED: Add paired_entity to INSERT query
        QString insertQuery = R"(
            INSERT INTO railway_control.point_machines
            (machine_id, machine_name, junction_row, junction_col,
             root_track_segment_connection, normal_track_segment_connection, reverse_track_segment_connection,
             current_position_id, operating_status, transition_time_ms, paired_entity)
            VALUES (?, ?, ?, ?, ?::jsonb, ?::jsonb, ?::jsonb, ?, ?, ?, ?)
        )";

        // UPDATED: Add paired_entity to params list
        QVariantList params = {
            point["id"].toString(),
            point["name"].toString(),
            point["junctionPoint"].toObject()["row"].toDouble(),
            point["junctionPoint"].toObject()["col"].toDouble(),
            rootTrackSegmentJson,
            normalTrackSegmentJson,
            reverseTrackSegmentJson,
            positionId,
            point["operatingStatus"].toString("CONNECTED"),
            3000, // Default transition time
            pairedEntity.isEmpty() ? QVariant() : pairedEntity // NEW: Paired entity (NULL if empty)
        };

        if (!executeQuery(insertQuery, params)) {
            setError(QString("Failed to insert point machine: %1").arg(point["id"].toString()));
            return false;
        }
    }

    return true;
}

bool DatabaseInitializer::populateTextLabels() {
    QJsonArray labelsData = getTextLabelsData();

    QString insertQuery = R"(
        INSERT INTO railway_control.text_labels
        (label_text, position_row, position_col, font_size)
        VALUES (?, ?, ?, ?)
    )";

    for (const auto& labelValue : labelsData) {
        QJsonObject label = labelValue.toObject();

        QVariantList params = {
            label["text"].toString(),
            label["row"].toDouble(),
            label["col"].toDouble(),
            label["fontSize"].toInt(12)
        };

        if (!executeQuery(insertQuery, params)) {
            return false;
        }
    }

    return true;
}

bool DatabaseInitializer::populateInterlockingRules() {
    updateProgress(60, "Populating interlocking rules...");

    QJsonArray rulesData = getInterlockingRulesData();

    QString insertQuery = R"(
        INSERT INTO railway_control.interlocking_rules (
            rule_name, source_entity_type, source_entity_id,
            target_entity_type, target_entity_id, target_constraint,
            rule_type, priority
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT DO NOTHING
    )";

    for (const auto& value : rulesData) {
        QJsonObject rule = value.toObject();

        QVariantList params = {
            rule["rule_name"].toString(),
            rule["source_entity_type"].toString(),
            rule["source_entity_id"].toString(),
            rule["target_entity_type"].toString(),
            rule["target_entity_id"].toString(),
            rule["target_constraint"].toString(),
            rule["rule_type"].toString(),
            rule["priority"].toInt()
        };

        if (!executeQuery(insertQuery, params)) {
            return false;
        }
    }

    qDebug() << "✅ Populated" << rulesData.size() << "interlocking rules from structured data";
    return true;
}

bool DatabaseInitializer::validateDatabase() {
    QStringList validationQueries = {
        "SELECT COUNT(*) FROM railway_control.track_circuits",
        "SELECT COUNT(*) FROM railway_control.track_segments",
        "SELECT COUNT(*) FROM railway_control.signals",
        "SELECT COUNT(*) FROM railway_control.point_machines",
        "SELECT COUNT(*) FROM railway_config.signal_types",
        "SELECT COUNT(*) FROM railway_config.signal_aspects"
    };

    for (const QString& query : validationQueries) {
        QSqlQuery validationQuery(db);
        if (!validationQuery.exec(query)) {
            setError(QString("Validation failed for query: %1").arg(query));
            return false;
        }

        if (validationQuery.next()) {
            int count = validationQuery.value(0).toInt();
            qDebug() << "Validation:" << query << "returned" << count << "rows";
            if (count == 0 && !query.contains("signal_types") && !query.contains("signal_aspects")) {
                setError(QString("Validation failed: No data found for %1").arg(query));
                return false;
            }
        }
    }

    return true;
}

// ✅ UPDATED: Advanced functions for circuit-based occupancy
bool DatabaseInitializer::createAdvancedFunctions() {
    QStringList advancedFunctions = {
        // ✅ UPDATED: Audit logging function with track_circuits support
        R"(CREATE OR REPLACE FUNCTION railway_audit.log_changes()
        RETURNS TRIGGER AS $$
        DECLARE
            entity_name_val VARCHAR(100);
            old_json JSONB;
            new_json JSONB;
            operator_id_val VARCHAR(100);
            operation_source_val VARCHAR(50);
        BEGIN
            -- Determine entity name based on table
            CASE TG_TABLE_NAME
                WHEN 'track_segments' THEN
                    entity_name_val := COALESCE(NEW.segment_name, OLD.segment_name, NEW.segment_id, OLD.segment_id);
                WHEN 'track_circuits' THEN
                    entity_name_val := COALESCE(NEW.circuit_name, OLD.circuit_name, NEW.circuit_id, OLD.circuit_id);
                WHEN 'signals' THEN
                    entity_name_val := COALESCE(NEW.signal_name, OLD.signal_name, NEW.signal_id, OLD.signal_id);
                WHEN 'point_machines' THEN
                    entity_name_val := COALESCE(NEW.machine_name, OLD.machine_name, NEW.machine_id, OLD.machine_id);
                ELSE
                    entity_name_val := 'Unknown';
            END CASE;

            -- Convert to JSON for comparison
            IF TG_OP != 'INSERT' THEN
                old_json := to_jsonb(OLD);
            END IF;
            IF TG_OP != 'DELETE' THEN
                new_json := to_jsonb(NEW);
            END IF;

            -- Get context variables with safe defaults
            BEGIN
                operator_id_val := current_setting('railway.operator_id');
            EXCEPTION WHEN OTHERS THEN
                operator_id_val := 'system';
            END;

            BEGIN
                operation_source_val := current_setting('railway.operation_source');
            EXCEPTION WHEN OTHERS THEN
                operation_source_val := 'HMI';
            END;

            -- Insert audit record
            INSERT INTO railway_audit.event_log (
                event_type,
                entity_type,
                entity_id,
                entity_name,
                old_values,
                new_values,
                operator_id,
                operation_source,
                safety_critical,
                replay_data,
                sequence_number
            ) VALUES (
                TG_OP,
                TG_TABLE_NAME,
                COALESCE(NEW.id::TEXT, OLD.id::TEXT),
                entity_name_val,
                old_json,
                new_json,
                operator_id_val,
                operation_source_val,
                CASE TG_TABLE_NAME
                    WHEN 'signals' THEN true
                    WHEN 'point_machines' THEN true
                    WHEN 'track_circuits' THEN true  -- ✅ NEW: Circuits are safety critical
                    ELSE false
                END,
                COALESCE(new_json, old_json),
                nextval('railway_audit.event_sequence')
            );

            RETURN COALESCE(NEW, OLD);
        END;
        $$ LANGUAGE plpgsql)",

        // ✅ NEW: Track Segment circuits notification function
        R"(CREATE OR REPLACE FUNCTION railway_control.notify_track_segment_circuit_changes()
        RETURNS TRIGGER AS $$
        DECLARE
            payload JSON;
        BEGIN
            payload := json_build_object(
                'table', 'track_circuits',
                'operation', TG_OP,
                'id', COALESCE(NEW.id, OLD.id),
                'circuit_id', COALESCE(NEW.circuit_id, OLD.circuit_id),
                'is_occupied', COALESCE(NEW.is_occupied, false),
                'timestamp', extract(epoch from now())
            );

            PERFORM pg_notify('railway_changes', payload::TEXT);
            RETURN COALESCE(NEW, OLD);
        END;
        $$ LANGUAGE plpgsql)",

        // Track Segment segments notification function
        R"(CREATE OR REPLACE FUNCTION railway_control.notify_track_segment_changes()
        RETURNS TRIGGER AS $$
        DECLARE
            payload JSON;
        BEGIN
            payload := json_build_object(
                'table', 'track_segments',
                'operation', TG_OP,
                'id', COALESCE(NEW.id, OLD.id),
                'entity_id', COALESCE(NEW.segment_id, OLD.segment_id),
                'timestamp', extract(epoch from now())
            );

            PERFORM pg_notify('railway_changes', payload::TEXT);
            RETURN COALESCE(NEW, OLD);
        END;
        $$ LANGUAGE plpgsql)",

        // ✅ NEW: Circuit-based occupancy update function
        R"(CREATE OR REPLACE FUNCTION railway_control.update_track_segment_circuit_occupancy(
            circuit_id_param VARCHAR,
            is_occupied_param BOOLEAN,
            occupied_by_param VARCHAR DEFAULT NULL,
            operator_id_param VARCHAR DEFAULT 'system'
        )
        RETURNS BOOLEAN AS $$
        DECLARE
            rows_affected INTEGER;
        BEGIN
            -- Set operator context for audit logging
            PERFORM set_config('railway.operator_id', operator_id_param, true);

            -- Update track_segment circuit occupancy
            UPDATE railway_control.track_circuits
            SET
                is_occupied = is_occupied_param,
                occupied_by = CASE
                    WHEN is_occupied_param = TRUE THEN occupied_by_param
                    ELSE NULL
                END,
                updated_at = CURRENT_TIMESTAMP
            WHERE circuit_id = circuit_id_param;

            GET DIAGNOSTICS rows_affected = ROW_COUNT;
            RETURN rows_affected > 0;
        END;
        $$ LANGUAGE plpgsql)",

        // ✅ UPDATED: Legacy trackSegment occupancy function (maps to circuit)
        R"(CREATE OR REPLACE FUNCTION railway_control.update_track_segment_occupancy(
            segment_id_param VARCHAR,
            is_occupied_param BOOLEAN,
            occupied_by_param VARCHAR DEFAULT NULL,
            operator_id_param VARCHAR DEFAULT 'system'
        )
        RETURNS BOOLEAN AS $$
        DECLARE
            circuit_id_val VARCHAR(20);
            circuit_result BOOLEAN;
        BEGIN
            -- Find the circuit ID for this segment
            SELECT circuit_id INTO circuit_id_val
            FROM railway_control.track_segments
            WHERE segment_id = segment_id_param;

            -- If no circuit found or circuit is INVALID, return false
            IF circuit_id_val IS NULL OR circuit_id_val = 'INVALID' THEN
                RETURN false;
            END IF;

            -- Update the circuit occupancy
            SELECT railway_control.update_track_segment_circuit_occupancy(
                circuit_id_val,
                is_occupied_param,
                occupied_by_param,
                operator_id_param
            ) INTO circuit_result;

            RETURN circuit_result;
        END;
        $$ LANGUAGE plpgsql)",

        // Other functions remain the same...
        R"(CREATE OR REPLACE FUNCTION railway_control.update_signal_aspect(
            signal_id_param VARCHAR,
            aspect_code_param VARCHAR,
            operator_id_param VARCHAR DEFAULT 'system'
        )
        RETURNS BOOLEAN AS $$
        DECLARE
            aspect_id_val INTEGER;
            rows_affected INTEGER;
        BEGIN
            PERFORM set_config('railway.operator_id', operator_id_param, true);
            aspect_id_val := railway_config.get_aspect_id(aspect_code_param);
            IF aspect_id_val IS NULL THEN
                RAISE EXCEPTION 'Invalid aspect code: %', aspect_code_param;
            END IF;
            UPDATE railway_control.signals
            SET current_aspect_id = aspect_id_val
            WHERE signal_id = signal_id_param;
            GET DIAGNOSTICS rows_affected = ROW_COUNT;
            RETURN rows_affected > 0;
        END;
        $$ LANGUAGE plpgsql)",

        R"(CREATE OR REPLACE FUNCTION railway_control.update_subsidiary_signal_aspect(
            signal_id_param VARCHAR,
            aspect_type_param VARCHAR,
            aspect_code_param VARCHAR,
            operator_id_param VARCHAR DEFAULT 'system'
        )
        RETURNS BOOLEAN AS $$
        DECLARE
            aspect_id_val INTEGER;
            rows_affected INTEGER;
            column_name VARCHAR;
        BEGIN
            -- Set operator context for audit logging
            PERFORM set_config('railway.operator_id', operator_id_param, true);

            -- Validate aspect type
            IF aspect_type_param NOT IN ('CALLING_ON', 'LOOP') THEN
                RAISE EXCEPTION 'Invalid subsidiary aspect type: %. Must be CALLING_ON or LOOP', aspect_type_param;
            END IF;

            -- Get aspect ID
            aspect_id_val := railway_config.get_aspect_id(aspect_code_param);
            IF aspect_id_val IS NULL THEN
                RAISE EXCEPTION 'Invalid aspect code: %', aspect_code_param;
            END IF;

            -- Determine which column to update
            IF aspect_type_param = 'CALLING_ON' THEN
                column_name := 'calling_on_aspect_id';
            ELSIF aspect_type_param = 'LOOP' THEN
                column_name := 'loop_aspect_id';
            END IF;

            -- Update the appropriate subsidiary signal column
            IF aspect_type_param = 'CALLING_ON' THEN
                UPDATE railway_control.signals
                SET calling_on_aspect_id = aspect_id_val,
                    last_changed_at = CURRENT_TIMESTAMP,
                    last_changed_by = operator_id_param
                WHERE signal_id = signal_id_param;
            ELSIF aspect_type_param = 'LOOP' THEN
                UPDATE railway_control.signals
                SET loop_aspect_id = aspect_id_val,
                    last_changed_at = CURRENT_TIMESTAMP,
                    last_changed_by = operator_id_param
                WHERE signal_id = signal_id_param;
            END IF;

            GET DIAGNOSTICS rows_affected = ROW_COUNT;

            -- Log the change for audit trail
            -- Future Reference

            RETURN rows_affected > 0;
        END;
        $$ LANGUAGE plpgsql)",

        R"(CREATE OR REPLACE FUNCTION railway_control.update_point_position(
            machine_id_param VARCHAR,
            position_code_param VARCHAR,
            operator_id_param VARCHAR DEFAULT 'system'
        )
        RETURNS BOOLEAN AS $$
        DECLARE
            position_id_val INTEGER;
            rows_affected INTEGER;
        BEGIN
            PERFORM set_config('railway.operator_id', operator_id_param, true);
            position_id_val := railway_config.get_position_id(position_code_param);
            IF position_id_val IS NULL THEN
                RAISE EXCEPTION 'Invalid position code: %', position_code_param;
            END IF;
            UPDATE railway_control.point_machines
            SET
                current_position_id = position_id_val,
                last_operated_at = CURRENT_TIMESTAMP,
                last_operated_by = operator_id_param,
                operation_count = operation_count + 1
            WHERE machine_id = machine_id_param;
            GET DIAGNOSTICS rows_affected = ROW_COUNT;
            RETURN rows_affected > 0;
        END;
        $$ LANGUAGE plpgsql)",

        R"(CREATE OR REPLACE FUNCTION railway_control.update_point_position_paired(
            machine_id_param VARCHAR,
            position_code_param VARCHAR,
            operator_id_param VARCHAR DEFAULT 'system'
        )
        RETURNS JSONB AS $$
        DECLARE
            position_id_val INTEGER;
            paired_machine_id VARCHAR(20);
            current_position_code VARCHAR(20);
            paired_current_position_code VARCHAR(20);
            rows_affected INTEGER;
            result_json JSONB;
            position_mismatch BOOLEAN := FALSE;
        BEGIN
            -- Set operator context for audit logging
            PERFORM set_config('railway.operator_id', operator_id_param, true);

            -- Validate position code
            position_id_val := railway_config.get_position_id(position_code_param);
            IF position_id_val IS NULL THEN
                RAISE EXCEPTION 'Invalid position code: %', position_code_param;
            END IF;

            -- Get current machine info including paired entity
            SELECT
                pp.position_code,
                pm.paired_entity
            INTO
                current_position_code,
                paired_machine_id
            FROM railway_control.point_machines pm
            LEFT JOIN railway_config.point_positions pp ON pm.current_position_id = pp.id
            WHERE pm.machine_id = machine_id_param;

            IF NOT FOUND THEN
                RAISE EXCEPTION 'Point machine not found: %', machine_id_param;
            END IF;

            -- Check if requesting same position (no-op)
            IF current_position_code = position_code_param THEN
                result_json := jsonb_build_object(
                    'success', true,
                    'machines_updated', ARRAY[machine_id_param],
                    'message', 'Already in requested position',
                    'position_mismatch', false
                );
                RETURN result_json;
            END IF;

            -- Handle unpaired machine (simple case)
            IF paired_machine_id IS NULL THEN
                UPDATE railway_control.point_machines
                SET
                    current_position_id = position_id_val,
                    last_operated_at = CURRENT_TIMESTAMP,
                    last_operated_by = operator_id_param,
                    operation_count = operation_count + 1
                WHERE machine_id = machine_id_param;

                GET DIAGNOSTICS rows_affected = ROW_COUNT;

                result_json := jsonb_build_object(
                    'success', rows_affected > 0,
                    'machines_updated', ARRAY[machine_id_param],
                    'message', 'Single point machine updated',
                    'position_mismatch', false
                );
                RETURN result_json;
            END IF;

            -- Handle paired machine
            -- Get paired machine current position
            SELECT pp.position_code
            INTO paired_current_position_code
            FROM railway_control.point_machines pm
            LEFT JOIN railway_config.point_positions pp ON pm.current_position_id = pp.id
            WHERE pm.machine_id = paired_machine_id;

            IF NOT FOUND THEN
                RAISE EXCEPTION 'Paired machine not found: %', paired_machine_id;
            END IF;

            -- Check for position mismatch
            IF current_position_code != paired_current_position_code THEN
                position_mismatch := TRUE;

                -- CRITICAL LOG: Position mismatch detected
                RAISE WARNING 'CRITICAL: Position mismatch detected between paired machines % (%) and % (%)',
                    machine_id_param, current_position_code,
                    paired_machine_id, paired_current_position_code;

                -- Update only requesting machine to match its pair
                UPDATE railway_control.point_machines
                SET
                    current_position_id = (
                        SELECT current_position_id
                        FROM railway_control.point_machines
                        WHERE machine_id = paired_machine_id
                    ),
                    last_operated_at = CURRENT_TIMESTAMP,
                    last_operated_by = operator_id_param,
                    operation_count = operation_count + 1
                WHERE machine_id = machine_id_param;

                GET DIAGNOSTICS rows_affected = ROW_COUNT;

                result_json := jsonb_build_object(
                    'success', rows_affected > 0,
                    'machines_updated', ARRAY[machine_id_param],
                    'message', 'Position mismatch corrected - machine synchronized with pair',
                    'position_mismatch', true,
                    'corrected_to_position', paired_current_position_code
                );
                RETURN result_json;
            END IF;

            -- Both machines have same position - update both atomically
            UPDATE railway_control.point_machines
            SET
                current_position_id = position_id_val,
                last_operated_at = CURRENT_TIMESTAMP,
                last_operated_by = operator_id_param,
                operation_count = operation_count + 1
            WHERE machine_id IN (machine_id_param, paired_machine_id);

            GET DIAGNOSTICS rows_affected = ROW_COUNT;

            result_json := jsonb_build_object(
                'success', rows_affected = 2,
                'machines_updated', ARRAY[machine_id_param, paired_machine_id],
                'message', 'Paired machines updated together',
                'position_mismatch', false
            );

            RETURN result_json;
        END;
        $$ LANGUAGE plpgsql)",

        R"(CREATE OR REPLACE FUNCTION railway_control.notify_signal_changes()
        RETURNS TRIGGER AS $$
        DECLARE
            payload JSON;
        BEGIN
            payload := json_build_object(
                'table', 'signals',
                'operation', TG_OP,
                'id', COALESCE(NEW.id, OLD.id),
                'entity_id', COALESCE(NEW.signal_id, OLD.signal_id),
                'timestamp', extract(epoch from now())
            );

            PERFORM pg_notify('railway_changes', payload::TEXT);
            RETURN COALESCE(NEW, OLD);
        END;
        $$ LANGUAGE plpgsql)",

        // ✅ ADD: Missing point machine notification function
        R"(CREATE OR REPLACE FUNCTION railway_control.notify_point_changes()
        RETURNS TRIGGER AS $$
        DECLARE
            payload JSON;
        BEGIN
            payload := json_build_object(
                'table', 'point_machines',
                'operation', TG_OP,
                'id', COALESCE(NEW.id, OLD.id),
                'entity_id', COALESCE(NEW.machine_id, OLD.machine_id),
                'timestamp', extract(epoch from now())
            );

            PERFORM pg_notify('railway_changes', payload::TEXT);
            RETURN COALESCE(NEW, OLD);
        END;
        $$ LANGUAGE plpgsql)",

        R"(CREATE OR REPLACE FUNCTION railway_control.update_track_segment_assignment(
            segment_id_param VARCHAR,
            is_assigned_param BOOLEAN,
            operator_id_param VARCHAR DEFAULT 'system'
        )
        RETURNS BOOLEAN AS $$
        DECLARE
            rows_affected INTEGER;
        BEGIN
            PERFORM set_config('railway.operator_id', operator_id_param, true);

            UPDATE railway_control.track_segments
            SET is_assigned = is_assigned_param
            WHERE segment_id = segment_id_param;

            GET DIAGNOSTICS rows_affected = ROW_COUNT;
            RETURN rows_affected > 0;
        END;
        $$ LANGUAGE plpgsql)",
        R"(CREATE OR REPLACE FUNCTION railway_control.get_system_status()
        RETURNS JSON AS $$
        DECLARE
            result JSON;
           track_segment_stats RECORD;
            circuit_stats RECORD;
            signal_stats RECORD;
            point_stats RECORD;
        BEGIN
            SELECT
                COUNT(*) as total,
                COUNT(*) FILTER (WHERE is_assigned) as assigned
            INTO track_segment_stats
            FROM railway_control.track_segments
            WHERE is_active = TRUE;

            SELECT
                COUNT(*) as total,
                COUNT(*) FILTER (WHERE is_occupied) as occupied
            INTO circuit_stats
            FROM railway_control.track_circuits
            WHERE is_active = TRUE;

            SELECT
                COUNT(*) as total,
                COUNT(*) FILTER (WHERE is_active) as active
            INTO signal_stats
            FROM railway_control.signals;

            SELECT
                COUNT(*) as total,
                COUNT(*) FILTER (WHERE operating_status = 'CONNECTED') as connected,
                COUNT(*) FILTER (WHERE operating_status = 'IN_TRANSITION') as in_transition
            INTO point_stats
            FROM railway_control.point_machines;

            result := json_build_object(
                'timestamp', extract(epoch from now()),
                'track_segments', json_build_object(
                    'total_segments', track_segment_stats.total,
                    'assigned_segments', track_segment_stats.assigned,
                    'total_circuits', circuit_stats.total,
                    'occupied_circuits', circuit_stats.occupied,
                    'available_segments', track_segment_stats.total - track_segment_stats.assigned
                ),
                'signals', json_build_object(
                    'total', signal_stats.total,
                    'active', signal_stats.active
                ),
                'point_machines', json_build_object(
                    'total', point_stats.total,
                    'connected', point_stats.connected,
                    'in_transition', point_stats.in_transition
                )
            );

            RETURN result;
        END;
        $$ LANGUAGE plpgsql)"
    };

    qDebug() << "Creating advanced functions...";
    for (const QString& query : advancedFunctions) {
        if (!executeQuery(query)) {
            qWarning() << "Failed to create advanced function:" << query.left(100) + "...";
        }
    }

    return true;
}

bool DatabaseInitializer::createAdvancedTriggers() {
    QStringList advancedTriggers = {
        // Audit triggers
        R"(CREATE TRIGGER trg_track_segments_audit
            AFTER INSERT OR UPDATE OR DELETE ON railway_control.track_segments
            FOR EACH ROW EXECUTE FUNCTION railway_audit.log_changes())",

        R"(CREATE TRIGGER trg_track_circuits_audit
            AFTER INSERT OR UPDATE OR DELETE ON railway_control.track_circuits
            FOR EACH ROW EXECUTE FUNCTION railway_audit.log_changes())",

        R"(CREATE TRIGGER trg_signals_audit
            AFTER INSERT OR UPDATE OR DELETE ON railway_control.signals
            FOR EACH ROW EXECUTE FUNCTION railway_audit.log_changes())",

        R"(CREATE TRIGGER trg_point_machines_audit
            AFTER INSERT OR UPDATE OR DELETE ON railway_control.point_machines
            FOR EACH ROW EXECUTE FUNCTION railway_audit.log_changes())",

        // Notification triggers
        R"(CREATE TRIGGER trg_track_segments_notify
            AFTER INSERT OR UPDATE OR DELETE ON railway_control.track_segments
            FOR EACH ROW EXECUTE FUNCTION railway_control.notify_track_segment_changes())",

        R"(CREATE TRIGGER trg_track_circuits_notify
            AFTER INSERT OR UPDATE OR DELETE ON railway_control.track_circuits
            FOR EACH ROW EXECUTE FUNCTION railway_control.notify_track_segment_circuit_changes())",

        R"(CREATE TRIGGER trg_signals_notify
            AFTER INSERT OR UPDATE OR DELETE ON railway_control.signals
            FOR EACH ROW EXECUTE FUNCTION railway_control.notify_signal_changes())",

        R"(CREATE TRIGGER trg_point_machines_notify
            AFTER INSERT OR UPDATE OR DELETE ON railway_control.point_machines
            FOR EACH ROW EXECUTE FUNCTION railway_control.notify_point_changes())"
    };

    qDebug() << "Creating advanced triggers...";
    for (const QString& query : advancedTriggers) {
        if (!executeQuery(query)) {
            qWarning() << "Failed to create advanced trigger:" << query.left(100) + "...";
        }
    }

    return true;
}

bool DatabaseInitializer::createGinIndexes() {
    QStringList ginIndexes = {
        "CREATE INDEX idx_signals_possible_aspects ON railway_control.signals USING gin(possible_aspects)",
        "CREATE INDEX idx_signals_interlocked_with ON railway_control.signals USING gin(interlocked_with)",
        "CREATE INDEX idx_point_machines_safety_interlocks ON railway_control.point_machines USING gin(safety_interlocks)",
        "CREATE INDEX idx_event_log_old_values ON railway_audit.event_log USING gin(old_values)",
        "CREATE INDEX idx_event_log_new_values ON railway_audit.event_log USING gin(new_values)",
        "CREATE INDEX idx_event_log_replay_data ON railway_audit.event_log USING gin(replay_data)",
        "CREATE INDEX idx_track_circuits_protecting_signals ON railway_control.track_circuits USING gin(protecting_signals)",
        "CREATE INDEX idx_interlocking_rules_source ON railway_control.interlocking_rules(source_entity_type, source_entity_id)",
        "CREATE INDEX idx_interlocking_rules_target ON railway_control.interlocking_rules(target_entity_type, target_entity_id)",
        "CREATE INDEX idx_signals_protected_track_circuits ON railway_control.signals USING gin(protected_track_circuits)",
        "CREATE INDEX idx_track_segments_protecting_signals ON railway_control.track_segments USING gin(protecting_signals)",
        "CREATE INDEX idx_point_machines_protected_signals ON railway_control.point_machines USING gin(protected_signals)"
    };

    qDebug() << "Creating GIN indexes...";
    for (const QString& query : ginIndexes) {
        if (!executeQuery(query)) {
            qWarning() << "Failed to create GIN index:" << query.left(80) + "...";
        }
    }

    return true;
}

// ✅ UPDATED: Views for circuit-based occupancy
bool DatabaseInitializer::createViews() {
    QStringList views = {
        // ✅ CRITICAL: Main view for segment occupancy from circuit occupancy
        R"(CREATE OR REPLACE VIEW railway_control.v_track_segments_with_occupancy AS
        SELECT
            ts.id,
            ts.segment_id,
            ts.segment_name,
            ts.start_row,
            ts.start_col,
            ts.end_row,
            ts.end_col,
            ts.track_segment_type,
            ts.is_assigned,
            ts.circuit_id,
            ts.length_meters,
            ts.max_speed_kmh,
            ts.is_active,
            ts.protecting_signals,
            ts.created_at,
            ts.updated_at,
            -- ✅ Get occupancy from circuit, not segment
            COALESCE(tc.is_occupied, false) as is_occupied,
            tc.occupied_by
        FROM railway_control.track_segments ts
        LEFT JOIN railway_control.track_circuits tc ON ts.circuit_id = tc.circuit_id)",

        // Complete signal information view
        R"(CREATE OR REPLACE VIEW railway_control.v_signals_complete AS
        SELECT
            s.id,
            s.signal_id,
            s.signal_name,
            st.type_code as signal_type,
            st.type_name as signal_type_name,
            s.location_row,
            s.location_col,
            s.direction,

            -- ✅ MAIN SIGNAL ASPECT (unchanged)
            sa_main.aspect_code as current_aspect,
            sa_main.aspect_name as current_aspect_name,
            sa_main.color_code as current_aspect_color,

            -- ✅ CALLING-ON SUBSIDIARY SIGNAL
            COALESCE(sa_calling.aspect_code, 'OFF') as calling_on_aspect,
            COALESCE(sa_calling.aspect_name, 'Off/Dark') as calling_on_aspect_name,
            COALESCE(sa_calling.color_code, '#404040') as calling_on_aspect_color,

            -- ✅ LOOP SUBSIDIARY SIGNAL
            COALESCE(sa_loop.aspect_code, 'OFF') as loop_aspect,
            COALESCE(sa_loop.aspect_name, 'Off/Dark') as loop_aspect_name,
            COALESCE(sa_loop.color_code, '#404040') as loop_aspect_color,

            -- ✅ SIGNAL CONFIGURATION (unchanged)
            s.loop_signal_configuration,
            s.aspect_count,
            s.possible_aspects,
            s.is_active,
            s.location_description,

            -- ✅ AUDIT FIELDS (unchanged)
            s.last_changed_at,
            s.last_changed_by,

            -- ✅ INTERLOCKING FIELDS (unchanged)
            s.interlocked_with,
            s.protected_track_circuits,
            s.manual_control_active,

            -- ✅ TIMESTAMP FIELDS (unchanged)
            s.created_at,
            s.updated_at

        FROM railway_control.signals s
        JOIN railway_config.signal_types st ON s.signal_type_id = st.id

        -- ✅ MAIN SIGNAL ASPECT JOIN (unchanged)
        LEFT JOIN railway_config.signal_aspects sa_main ON s.current_aspect_id = sa_main.id

        -- ✅ CALLING-ON ASPECT JOIN (new)
        LEFT JOIN railway_config.signal_aspects sa_calling ON s.calling_on_aspect_id = sa_calling.id

        -- ✅ LOOP ASPECT JOIN (new)
        LEFT JOIN railway_config.signal_aspects sa_loop ON s.loop_aspect_id = sa_loop.id)",

        // Complete point machine information view
        R"(CREATE VIEW railway_control.v_point_machines_complete AS
        SELECT
            pm.id,
            pm.machine_id,
            pm.machine_name,
            pm.junction_row,
            pm.junction_col,
            pm.root_track_segment_connection,
            pm.normal_track_segment_connection,
            pm.reverse_track_segment_connection,
            pp.position_code as current_position,
            pp.position_name as current_position_name,
            pm.operating_status,
            pm.transition_time_ms,
            pm.last_operated_at,
            pm.last_operated_by,
            pm.operation_count,
            pm.is_locked,
            pm.lock_reason,
            pm.created_at,
            pm.updated_at
        FROM railway_control.point_machines pm
        LEFT JOIN railway_config.point_positions pp ON pm.current_position_id = pp.id)",

        // ✅ UPDATED: Track Segment occupancy summary using circuits
        R"(CREATE VIEW railway_control.v_track_segment_occupancy AS
        SELECT
            COUNT(DISTINCT ts.segment_id) as total_segments,
            COUNT(DISTINCT ts.segment_id) FILTER (WHERE tc.is_occupied = true) as occupied_count,
            COUNT(DISTINCT ts.segment_id) FILTER (WHERE ts.is_assigned = true) as assigned_count,
            COUNT(DISTINCT ts.segment_id) FILTER (WHERE tc.is_occupied = true OR ts.is_assigned = true) as unavailable_count,
            ROUND(
                (COUNT(DISTINCT ts.segment_id) FILTER (WHERE tc.is_occupied = true OR ts.is_assigned = true)::NUMERIC /
                 COUNT(DISTINCT ts.segment_id)) * 100,
                2
            ) as utilization_percentage
        FROM railway_control.track_segments ts
        LEFT JOIN railway_control.track_circuits tc ON ts.circuit_id = tc.circuit_id
        WHERE ts.is_active = TRUE)",

        // Recent events view
        R"(CREATE VIEW railway_audit.v_recent_events AS
        SELECT
            el.id,
            el.event_timestamp,
            el.event_type,
            el.entity_type,
            el.entity_id,
            el.entity_name,
            el.operator_id,
            el.operation_source,
            el.safety_critical,
            el.comments
        FROM railway_audit.event_log el
        WHERE el.event_timestamp >= (CURRENT_TIMESTAMP - INTERVAL '24 hours')
        ORDER BY el.event_timestamp DESC)"
    };

    qDebug() << "Creating views...";
    for (const QString& query : views) {
        if (!executeQuery(query)) {
            qWarning() << "Failed to create view:" << query.left(100) + "...";
        }
    }

    return true;
}

bool DatabaseInitializer::setupRolePermissions() {
    QStringList rolePermissions = {
        // Railway Control Operator
        "GRANT USAGE ON SCHEMA railway_control TO railway_operator",
        "GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA railway_control TO railway_operator",
        "GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA railway_control TO railway_operator",
        "GRANT USAGE ON SCHEMA railway_config TO railway_operator",
        "GRANT SELECT ON ALL TABLES IN SCHEMA railway_config TO railway_operator",
        "GRANT INSERT, UPDATE ON ALL TABLES IN SCHEMA railway_audit TO railway_operator",

        // Railway Observer
        "GRANT USAGE ON SCHEMA railway_control TO railway_observer",
        "GRANT SELECT ON ALL TABLES IN SCHEMA railway_control TO railway_observer",
        "GRANT USAGE ON SCHEMA railway_config TO railway_observer",
        "GRANT SELECT ON ALL TABLES IN SCHEMA railway_config TO railway_observer",
        "GRANT SELECT ON ALL TABLES IN SCHEMA railway_audit TO railway_observer",

        // Railway Auditor
        "GRANT USAGE ON SCHEMA railway_audit TO railway_auditor",
        "GRANT SELECT ON ALL TABLES IN SCHEMA railway_audit TO railway_auditor"
    };

    qDebug() << "Setting up role permissions...";
    for (const QString& query : rolePermissions) {
        if (!executeQuery(query)) {
            qWarning() << "Failed to grant permission:" << query.left(80) + "...";
        }
    }

    return true;
}

// Helper methods remain the same...
int DatabaseInitializer::insertSignalType(const QString& typeCode, const QString& typeName, int maxAspects) {
    QString query = R"(
        INSERT INTO railway_config.signal_types (type_code, type_name, max_aspects)
        VALUES (?, ?, ?) RETURNING id
    )";

    QSqlQuery sqlQuery(db);
    sqlQuery.prepare(query);
    sqlQuery.addBindValue(typeCode);
    sqlQuery.addBindValue(typeName);
    sqlQuery.addBindValue(maxAspects);

    if (sqlQuery.exec() && sqlQuery.next()) {
        return sqlQuery.value(0).toInt();
    }

    setError(QString("Failed to insert signal type: %1").arg(typeCode));
    return -1;
}

int DatabaseInitializer::insertSignalAspect(const QString& aspectCode, const QString& aspectName, const QString& colorCode, int safetyLevel) {
    QString query = R"(
        INSERT INTO railway_config.signal_aspects (aspect_code, aspect_name, color_code, safety_level)
        VALUES (?, ?, ?, ?) RETURNING id
    )";

    QSqlQuery sqlQuery(db);
    sqlQuery.prepare(query);
    sqlQuery.addBindValue(aspectCode);
    sqlQuery.addBindValue(aspectName);
    sqlQuery.addBindValue(colorCode);
    sqlQuery.addBindValue(safetyLevel);

    if (sqlQuery.exec() && sqlQuery.next()) {
        return sqlQuery.value(0).toInt();
    }

    return -1;
}

int DatabaseInitializer::insertPointPosition(const QString& positionCode, const QString& positionName) {
    QString query = R"(
        INSERT INTO railway_config.point_positions (position_code, position_name)
        VALUES (?, ?) RETURNING id
    )";

    QSqlQuery sqlQuery(db);
    sqlQuery.prepare(query);
    sqlQuery.addBindValue(positionCode);
    sqlQuery.addBindValue(positionName);

    if (sqlQuery.exec() && sqlQuery.next()) {
        return sqlQuery.value(0).toInt();
    }

    return -1;
}

bool DatabaseInitializer::isDatabaseConnected() {
    return db.isOpen() && db.isValid();
}

QVariantMap DatabaseInitializer::getDatabaseStatus() {
    QVariantMap status;
    status["connected"] = isDatabaseConnected();
    status["lastError"] = m_lastError;

    if (!isDatabaseConnected()) {
        return status;
    }

    // Get table counts
    QStringList tables = {"track_circuits", "track_segments", "signals", "point_machines", "text_labels"};
    for (const QString& table : tables) {
        QSqlQuery query(db);
        if (query.exec(QString("SELECT COUNT(*) FROM railway_control.%1").arg(table))) {
            if (query.next()) {
                status[table + "_count"] = query.value(0).toInt();
            }
        }
    }

    return status;
}

void DatabaseInitializer::testConnection() {
    bool success = connectToDatabase();
    QString message = success ? "Database connection successful" : m_lastError;
    emit connectionTestCompleted(success, message);
}

// Helper Methods
bool DatabaseInitializer::executeQuery(const QString& query, const QVariantList& params) {
    QSqlQuery sqlQuery(db);
    sqlQuery.prepare(query);

    for (const QVariant& param : params) {
        sqlQuery.addBindValue(param);
    }

    if (!sqlQuery.exec()) {
        setError(QString("Query failed: %1 - Error: %2").arg(query.left(50), sqlQuery.lastError().text()));
        return false;
    }

    return true;
}

void DatabaseInitializer::setError(const QString& error) {
    m_lastError = error;
    emit lastErrorChanged();
    qWarning() << "DatabaseInitializer Error:" << error;
}

void DatabaseInitializer::updateProgress(int value, const QString& operation) {
    m_progress = value;
    m_currentOperation = operation;
    emit progressChanged();
    emit currentOperationChanged();
    qDebug() << QString("Progress [%1%]: %2").arg(value).arg(operation);
}

QJsonArray DatabaseInitializer::getInterlockingRulesData() {
    return QJsonArray {
        // ✅ PROTECTION RULES
        QJsonObject{{"rule_name", "Signal AS002 protects Circuit A42T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "AS002"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "A42T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},

        QJsonObject{{"rule_name", "Signal OT001 protects Circuit 6T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "OT001"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "6T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},
        QJsonObject{{"rule_name", "Signal AS002 protects Circuit 6T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "AS002"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "6T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},

        QJsonObject{{"rule_name", "Signal OT001 protects Circuit 5T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "OT001"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "5T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},
        QJsonObject{{"rule_name", "Signal ST003 protects Circuit 5T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "ST003"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "5T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},

        QJsonObject{{"rule_name", "Signal HM001 protects Circuit W22T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "HM001"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "W22T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},
        QJsonObject{{"rule_name", "Signal ST003 protects Circuit W22T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "ST003"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "W22T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},
        QJsonObject{{"rule_name", "Signal ST004 protects Circuit W22T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "ST004"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "W22T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},

        QJsonObject{{"rule_name", "Signal HM001 protects Circuit 3T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "HM001"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "3T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},
        QJsonObject{{"rule_name", "Signal HM002 protects Circuit 3T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "HM002"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "3T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},

        QJsonObject{{"rule_name", "Signal HM002 protects Circuit W21T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "HM002"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "W21T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},
        QJsonObject{{"rule_name", "Signal ST001 protects Circuit W21T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "ST001"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "W21T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},
        QJsonObject{{"rule_name", "Signal ST002 protects Circuit W21T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "ST002"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "W21T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},

        QJsonObject{{"rule_name", "Signal OT002 protects Circuit 2T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "OT002"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "2T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},
        QJsonObject{{"rule_name", "Signal ST001 protects Circuit 2T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "ST001"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "2T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},

        QJsonObject{{"rule_name", "Signal OT002 protects Circuit 1T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "OT002"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "1T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},
        QJsonObject{{"rule_name", "Signal AS001 protects Circuit 1T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "AS001"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "1T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},

        QJsonObject{{"rule_name", "Signal AS001 protects Circuit A1T"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "AS001"}, {"target_entity_type", "TRACK_CIRCUIT"}, {"target_entity_id", "A1T"}, {"target_constraint", "MUST_BE_CLEAR"}, {"rule_type", "PROTECTING"}, {"priority", 900}},

        // ✅ OPPOSING RULES
        QJsonObject{{"rule_name", "Opposing Signals HM001-HM002"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "HM001"}, {"target_entity_type", "SIGNAL"}, {"target_entity_id", "HM002"}, {"target_constraint", "MUST_BE_RED"}, {"rule_type", "OPPOSING"}, {"priority", 1000}},
        QJsonObject{{"rule_name", "Opposing Signals HM002-HM001"}, {"source_entity_type", "SIGNAL"}, {"source_entity_id", "HM002"}, {"target_entity_type", "SIGNAL"}, {"target_entity_id", "HM001"}, {"target_constraint", "MUST_BE_RED"}, {"rule_type", "OPPOSING"}, {"priority", 1000}}
    };
}

// ✅ UPDATED: Data methods with circuit_id
QJsonArray DatabaseInitializer::getTrackSegmentsData() {
    return QJsonArray {
        QJsonObject{{"id", "T1S1"}, {"startRow", 110}, {"startCol", 0}, {"endRow", 110}, {"endCol", 12}, {"circuit_id", "INVALID"}, {"assigned", false}, {"protecting_signals", QJsonArray{}}},
        QJsonObject{{"id", "T1S2"}, {"startRow", 110}, {"startCol", 13}, {"endRow", 110}, {"endCol", 34}, {"circuit_id", "A42T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"AS002"}}},
        QJsonObject{{"id", "T1S3"}, {"startRow", 110}, {"startCol", 35}, {"endRow", 110}, {"endCol", 67}, {"circuit_id", "6T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"OT001", "AS002"}}},
        QJsonObject{{"id", "T1S4"}, {"startRow", 110}, {"startCol", 68}, {"endRow", 110}, {"endCol", 90}, {"circuit_id", "5T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"OT001", "ST003"}}},
        QJsonObject{{"id", "T1S5"}, {"startRow", 110}, {"startCol", 91}, {"endRow", 110}, {"endCol", 117}, {"circuit_id", "W22T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"HM001", "ST003", "ST004"}}},
        QJsonObject{{"id", "T1S6"}, {"startRow", 110}, {"startCol", 128}, {"endRow", 110}, {"endCol", 158}, {"circuit_id", "W22T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"HM001", "ST003", "ST004"}}},
        QJsonObject{{"id", "T1S7"}, {"startRow", 110}, {"startCol", 159}, {"endRow", 110}, {"endCol", 221}, {"circuit_id", "3T"}, {"assigned", false}, {"protecting_signals", QJsonArray{}}},
        QJsonObject{{"id", "T1S8"}, {"startRow", 110}, {"startCol", 222}, {"endRow", 110}, {"endCol", 254}, {"circuit_id", "W21T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"HM002", "ST001", "ST002"}}},
        QJsonObject{{"id", "T1S9"}, {"startRow", 110}, {"startCol", 264}, {"endRow", 110}, {"endCol", 286}, {"circuit_id", "W21T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"HM002", "ST001", "ST002"}}},
        QJsonObject{{"id", "T1S10"}, {"startRow", 110}, {"startCol", 287}, {"endRow", 110}, {"endCol", 305}, {"circuit_id", "2T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"OT002", "ST001"}}},
        QJsonObject{{"id", "T1S11"}, {"startRow", 110}, {"startCol", 306}, {"endRow", 110}, {"endCol", 338}, {"circuit_id", "1T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"OT002", "AS001"}}},
        QJsonObject{{"id", "T1S12"}, {"startRow", 110}, {"startCol", 339}, {"endRow", 110}, {"endCol", 358}, {"circuit_id", "A1T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"AS001"}}},
        QJsonObject{{"id", "T1S13"}, {"startRow", 110}, {"startCol", 359}, {"endRow", 110}, {"endCol", 369}, {"circuit_id", "INVALID"}, {"assigned", false}, {"protecting_signals", QJsonArray{}}},
        QJsonObject{{"id", "T4S1"}, {"startRow", 88}, {"startCol", 125}, {"endRow", 88}, {"endCol", 137}, {"circuit_id", "W22T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"HM001", "ST003", "ST004"}}},
        QJsonObject{{"id", "T4S2"}, {"startRow", 88}, {"startCol", 147}, {"endRow", 88}, {"endCol", 153}, {"circuit_id", "W22T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"HM001", "ST003", "ST004"}}},
        QJsonObject{{"id", "T4S3"}, {"startRow", 88}, {"startCol", 154}, {"endRow", 88}, {"endCol", 226}, {"circuit_id", "4T"}, {"assigned", false}, {"protecting_signals", QJsonArray{}}},
        QJsonObject{{"id", "T4S4"}, {"startRow", 88}, {"startCol", 227}, {"endRow", 88}, {"endCol", 232}, {"circuit_id", "W21T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"HM002", "ST001", "ST002"}}},
        QJsonObject{{"id", "T4S5"}, {"startRow", 88}, {"startCol", 242}, {"endRow", 88}, {"endCol", 258}, {"circuit_id", "W21T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"HM002", "ST001", "ST002"}}},
        QJsonObject{{"id", "T5S1"}, {"startRow", 106}, {"startCol", 125}, {"endRow", 92}, {"endCol", 139}, {"circuit_id", "W22T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"HM001", "ST003", "ST004"}}},
        QJsonObject{{"id", "T6S1"}, {"startRow", 92}, {"startCol", 240}, {"endRow", 105}, {"endCol", 254}, {"circuit_id", "W21T"}, {"assigned", false}, {"protecting_signals", QJsonArray{"HM002", "ST001", "ST002"}}}
    };
}

// ✅ NEW: Circuit mapping data
QJsonArray DatabaseInitializer::getTrackCircuitMappings() {
    return QJsonArray {
        QJsonObject{{"circuit_id", "A42T"}, {"circuit_name", "Approach Block A42T"}, {"protecting_signals", QJsonArray{"AS002"}}},
        QJsonObject{{"circuit_id", "6T"}, {"circuit_name", "Main Line Section 6T"}, {"protecting_signals", QJsonArray{"OT001", "AS002"}}},
        QJsonObject{{"circuit_id", "5T"}, {"circuit_name", "Main Line Section 5T"}, {"protecting_signals", QJsonArray{"OT001", "ST003"}}},
        QJsonObject{{"circuit_id", "W22T"}, {"circuit_name", "Junction W22T Circuit"}, {"protecting_signals", QJsonArray{"HM001", "ST003", "ST004"}}},
        QJsonObject{{"circuit_id", "3T"}, {"circuit_name", "Platform Section 3T"}, {"protecting_signals", QJsonArray{}}},
        QJsonObject{{"circuit_id", "W21T"}, {"circuit_name", "Junction W21T Circuit"}, {"protecting_signals", QJsonArray{"HM002", "ST001", "ST002"}}},
        QJsonObject{{"circuit_id", "2T"}, {"circuit_name", "Main Line Section 2T"}, {"protecting_signals", QJsonArray{"OT002", "ST001"}}},
        QJsonObject{{"circuit_id", "1T"}, {"circuit_name", "Main Line Section 1T"}, {"protecting_signals", QJsonArray{"OT002", "AS001"}}},
        QJsonObject{{"circuit_id", "A1T"}, {"circuit_name", "Exit Block A1T"}, {"protecting_signals", QJsonArray{"AS001"}}},
        QJsonObject{{"circuit_id", "4T"}, {"circuit_name", "Loop Section 4T"}, {"protecting_signals", QJsonArray{}}}
    };
}

// Signal and other data methods remain the same...
QJsonArray DatabaseInitializer::getOuterSignalsData() {
    return QJsonArray {
        QJsonObject{
            {"id", "OT001"}, {"name", "Outer A1"}, {"type", "OUTER"},
            {"row", 102}, {"col", 30}, {"direction", "UP"},
            {"currentAspect", "RED"}, {"aspectCount", 4},
            {"possibleAspects", QJsonArray{"RED", "SINGLE_YELLOW", "DOUBLE_YELLOW", "GREEN"}},
            {"protectedTrackCircuits", QJsonArray{"6T", "5T"}},  // ✅ ADDED
            {"isActive", true}, {"location", "Approach_Block_1"}
        },
        QJsonObject{
            {"id", "OT002"}, {"name", "Outer A2"}, {"type", "OUTER"},
            {"row", 113}, {"col", 330}, {"direction", "DOWN"},
            {"currentAspect", "RED"}, {"aspectCount", 4},
            {"possibleAspects", QJsonArray{"RED", "SINGLE_YELLOW", "DOUBLE_YELLOW", "GREEN"}},
            {"protectedTrackCircuits", QJsonArray{"2T", "1T"}},  // ✅ ADDED
            {"isActive", true}, {"location", "Approach_Block_2"}
        }
    };
}

QJsonArray DatabaseInitializer::getHomeSignalsData() {
    return QJsonArray {
        QJsonObject{
            {"id", "HM001"}, {"name", "Home A1"}, {"type", "HOME"},
            {"row", 102}, {"col", 84}, {"direction", "UP"},
            {"currentAspect", "RED"}, {"aspectCount", 3},
            {"possibleAspects", QJsonArray{"RED", "YELLOW", "GREEN"}},
            {"callingOnAspect", "WHITE"}, {"loopAspect", "YELLOW"}, {"loopSignalConfiguration", "UR"},
            {"protectedTrackCircuits", QJsonArray{"W22T"}},  // ✅ ADDED
            {"isActive", true}, {"location", "Platform_A_Entry"}
        },
        QJsonObject{
            {"id", "HM002"}, {"name", "Home A2"}, {"type", "HOME"},
            {"row", 113}, {"col", 275}, {"direction", "DOWN"},
            {"currentAspect", "RED"}, {"aspectCount", 3},
            {"possibleAspects", QJsonArray{"RED", "YELLOW", "GREEN"}},
            {"callingOnAspect", "OFF"}, {"loopAspect", "OFF"}, {"loopSignalConfiguration", "UR"},
            {"protectedTrackCircuits", QJsonArray{"W21T"}},  // ✅ ADDED
            {"isActive", true}, {"location", "Platform_A_Exit"}
        }
    };
}

QJsonArray DatabaseInitializer::getStarterSignalsData() {
    return QJsonArray {
        QJsonObject{
            {"id", "ST001"}, {"name", "Starter A2"}, {"type", "STARTER"},
            {"row", 103}, {"col", 217}, {"direction", "UP"},
            {"currentAspect", "RED"}, {"aspectCount", 3},
            {"possibleAspects", QJsonArray{"RED", "YELLOW", "GREEN"}},
            {"protectedTrackCircuits", QJsonArray{"W21T", "2T"}},  // ✅ ADDED
            {"isActive", true}, {"location", "Platform_A_Main_Departure"}
        },
        QJsonObject{
            {"id", "ST002"}, {"name", "Starter A1"}, {"type", "STARTER"},
            {"row", 83}, {"col", 220}, {"direction", "UP"},
            {"currentAspect", "RED"}, {"aspectCount", 2},
            {"possibleAspects", QJsonArray{"RED", "YELLOW"}},
            {"protectedTrackCircuits", QJsonArray{"W21T"}},  // ✅ ADDED
            {"isActive", true}, {"location", "Platform_A_Departure"}
        },
        QJsonObject{
            {"id", "ST003"}, {"name", "Starter B2"}, {"type", "STARTER"},
            {"row", 115}, {"col", 152}, {"direction", "DOWN"},
            {"currentAspect", "RED"}, {"aspectCount", 3},
            {"possibleAspects", QJsonArray{"RED", "YELLOW", "GREEN"}},
            {"protectedTrackCircuits", QJsonArray{"5T", "W22T"}},  // ✅ ADDED
            {"isActive", true}, {"location", "Platform_A_Main_Departure"}
        },
        QJsonObject{
            {"id", "ST004"}, {"name", "Starter B1"}, {"type", "STARTER"},
            {"row", 91}, {"col", 150}, {"direction", "DOWN"},
            {"currentAspect", "RED"}, {"aspectCount", 2},
            {"possibleAspects", QJsonArray{"RED", "YELLOW"}},
            {"protectedTrackCircuits", QJsonArray{"W22T"}},  // ✅ ADDED
            {"isActive", true}, {"location", "Junction_Loop_Entry"}
        }
    };
}

QJsonArray DatabaseInitializer::getAdvancedStarterSignalsData() {
    return QJsonArray {
        QJsonObject{
            {"id", "AS001"}, {"name", "Advanced Starter A1"}, {"type", "ADVANCED_STARTER"},
            {"row", 102}, {"col", 302}, {"direction", "UP"},
            {"currentAspect", "RED"}, {"aspectCount", 2},
            {"possibleAspects", QJsonArray{"RED", "GREEN"}},
            {"protectedTrackCircuits", QJsonArray{"1T", "A1T"}},  // ✅ ADDED
            {"isActive", true}, {"location", "Advanced_Departure_A"}
        },
        QJsonObject{
            {"id", "AS002"}, {"name", "Advanced Starter A2"}, {"type", "ADVANCED_STARTER"},
            {"row", 113}, {"col", 56}, {"direction", "DOWN"},
            {"currentAspect", "RED"}, {"aspectCount", 2},
            {"possibleAspects", QJsonArray{"RED", "GREEN"}},
            {"protectedTrackCircuits", QJsonArray{"A42T", "6T"}},  // ✅ ADDED
            {"isActive", true}, {"location", "Advanced_Departure_B"}
        }
    };
}

QJsonArray DatabaseInitializer::getPointMachinesData() {
    return QJsonArray {
        QJsonObject{
            {"id", "PM001"}, {"name", "Junction A"}, {"position", "NORMAL"}, {"operatingStatus", "CONNECTED"},
            {"pairedEntity", "PM002"}, // NEW: Paired with PM002
            {"junctionPoint", QJsonObject{{"row", 110}, {"col", 121.2}}},
            {"rootTrackSegment", QJsonObject{{"trackSegmentId", "T1S5"}, {"connectionEnd", "END"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}},
            {"normalTrackSegment", QJsonObject{{"trackSegmentId", "T1S6"}, {"connectionEnd", "START"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}},
            {"reverseTrackSegment", QJsonObject{{"trackSegmentId", "T5S1"}, {"connectionEnd", "START"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}}
        },
        QJsonObject{
            {"id", "PM002"}, {"name", "Junction B"}, {"position", "NORMAL"}, {"operatingStatus", "CONNECTED"},
            {"pairedEntity", "PM001"}, // NEW: Paired with PM001 (bidirectional)
            {"junctionPoint", QJsonObject{{"row", 88}, {"col", 143.3}}},
            {"rootTrackSegment", QJsonObject{{"trackSegmentId", "T4S2"}, {"connectionEnd", "START"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}},
            {"normalTrackSegment", QJsonObject{{"trackSegmentId", "T4S1"}, {"connectionEnd", "END"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}},
            {"reverseTrackSegment", QJsonObject{{"trackSegmentId", "T5S1"}, {"connectionEnd", "END"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}}
        },
        QJsonObject{
            {"id", "PM003"}, {"name", "Junction C"}, {"position", "NORMAL"}, {"operatingStatus", "CONNECTED"},
            {"pairedEntity", "PM004"}, // NEW: Paired with PM004
            {"junctionPoint", QJsonObject{{"row", 88}, {"col", 235.6}}},
            {"rootTrackSegment", QJsonObject{{"trackSegmentId", "T4S4"}, {"connectionEnd", "END"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}},
            {"normalTrackSegment", QJsonObject{{"trackSegmentId", "T4S5"}, {"connectionEnd", "START"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}},
            {"reverseTrackSegment", QJsonObject{{"trackSegmentId", "T6S1"}, {"connectionEnd", "START"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}}
        },
        QJsonObject{
            {"id", "PM004"}, {"name", "Junction D"}, {"position", "NORMAL"}, {"operatingStatus", "CONNECTED"},
            {"pairedEntity", "PM003"}, // NEW: Paired with PM003 (bidirectional)
            {"junctionPoint", QJsonObject{{"row", 110}, {"col", 259.5}}},
            {"rootTrackSegment", QJsonObject{{"trackSegmentId", "T1S9"}, {"connectionEnd", "START"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}},
            {"normalTrackSegment", QJsonObject{{"trackSegmentId", "T1S8"}, {"connectionEnd", "END"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}},
            {"reverseTrackSegment", QJsonObject{{"trackSegmentId", "T6S1"}, {"connectionEnd", "END"}, {"offset", QJsonObject{{"row", 0}, {"col", 0}}}}}
        }
    };
}

QJsonArray DatabaseInitializer::getTextLabelsData() {
    return QJsonArray {
        QJsonObject{{"text", "50"}, {"row", 1}, {"col", 49}, {"fontSize", 12}},
        QJsonObject{{"text", "100"}, {"row", 1}, {"col", 99}, {"fontSize", 12}},
        QJsonObject{{"text", "150"}, {"row", 1}, {"col", 149}, {"fontSize", 12}},
        QJsonObject{{"text", "200"}, {"row", 1}, {"col", 199}, {"fontSize", 12}},
        QJsonObject{{"text", "30"}, {"row", 29}, {"col", 1}, {"fontSize", 12}},
        QJsonObject{{"text", "90"}, {"row", 89}, {"col", 1}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S1"}, {"row", 107}, {"col", 4}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S2"}, {"row", 107}, {"col", 20}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S3"}, {"row", 107}, {"col", 48}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S4"}, {"row", 107}, {"col", 77}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S5"}, {"row", 107}, {"col", 105}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S6"}, {"row", 107}, {"col", 138}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S7"}, {"row", 107}, {"col", 188}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S8"}, {"row", 107}, {"col", 236}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S9"}, {"row", 107}, {"col", 271}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S10"}, {"row", 107}, {"col", 293}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S11"}, {"row", 107}, {"col", 318}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S12"}, {"row", 107}, {"col", 345}, {"fontSize", 12}},
        QJsonObject{{"text", "T1S13"}, {"row", 107}, {"col", 360}, {"fontSize", 12}},
        QJsonObject{{"text", "T4S1"}, {"row", 85}, {"col", 130}, {"fontSize", 12}},
        QJsonObject{{"text", "T4S3"}, {"row", 85}, {"col", 188}, {"fontSize", 12}},
        QJsonObject{{"text", "T4S5"}, {"row", 85}, {"col", 246}, {"fontSize", 12}}
    };
}

// ============================================================================
// ROUTE ASSIGNMENT SCHEMA EXTENSION METHODS
// ============================================================================

bool DatabaseInitializer::executeRouteAssignmentSchema() {
    qDebug() << "🔄 DatabaseInitializer: Installing route assignment schema extensions...";
    
    // Read the route assignment schema file
    QFile schemaFile(":/sql/route_assignment_schema_extensions.sql");
    if (!schemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Try alternative path
        schemaFile.setFileName("sql/route_assignment_schema_extensions.sql");
        if (!schemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            setError("Could not open route assignment schema file");
            return false;
        }
    }
    
    QTextStream in(&schemaFile);
    QString schemaContent = in.readAll();
    schemaFile.close();
    
    if (schemaContent.isEmpty()) {
        setError("Route assignment schema file is empty");
        return false;
    }
    
    // Split the schema into individual statements
    QStringList statements = schemaContent.split(";", Qt::SkipEmptyParts);
    
    qDebug() << "📋 DatabaseInitializer: Executing" << statements.size() << "route assignment schema statements...";
    
    // Execute each statement
    for (const QString& statement : statements) {
        QString cleanStatement = statement.trimmed();
        if (cleanStatement.isEmpty() || 
            cleanStatement.startsWith("--") || 
            cleanStatement.startsWith("/*") ||
            cleanStatement.toUpper().startsWith("BEGIN") ||
            cleanStatement.toUpper().startsWith("COMMIT")) {
            continue; // Skip comments and transaction control
        }
        
        if (!executeQuery(cleanStatement)) {
            qWarning() << "⚠️ DatabaseInitializer: Failed to execute route assignment statement:" << cleanStatement.left(100) + "...";
            qWarning() << "Last error:" << m_lastError;
            // Continue with other statements rather than failing completely
        }
    }
    
    qDebug() << "✅ DatabaseInitializer: Route assignment schema extensions installed";
    return true;
}

bool DatabaseInitializer::populateRouteAssignmentData() {
    qDebug() << "🔄 DatabaseInitializer: Populating route assignment initial data...";
    
    try {
        // Populate signal adjacency anchors for pathfinding
        if (!populateSignalAdjacencyAnchors()) {
            qWarning() << "⚠️ Failed to populate signal adjacency anchors";
            return false;
        }
        
        // Populate track circuit edges for pathfinding
        if (!populateTrackCircuitEdges()) {
            qWarning() << "⚠️ Failed to populate track circuit edges";
            return false;
        }
        
        // Populate signal overlap definitions
        if (!populateSignalOverlapDefinitions()) {
            qWarning() << "⚠️ Failed to populate signal overlap definitions";
            return false;
        }
        
        qDebug() << "✅ DatabaseInitializer: Route assignment data population completed";
        return true;
        
    } catch (const std::exception& e) {
        setError(QString("Failed to populate route assignment data: %1").arg(e.what()));
        return false;
    }
}

bool DatabaseInitializer::populateSignalAdjacencyAnchors() {
    qDebug() << "🔄 Populating signal adjacency anchors for pathfinding...";
    
    // Signal pathfinding anchors mapping - manually defined based on station layout
    QJsonArray anchorMappings = QJsonArray {
        // HOME signals
        QJsonObject{{"signal_id", "HM001"}, {"preceded_by", "W22T"}, {"succeeded_by", "W22T"}},
        QJsonObject{{"signal_id", "HM002"}, {"preceded_by", "W21T"}, {"succeeded_by", "W21T"}},
        
        // STARTER signals  
        QJsonObject{{"signal_id", "ST001"}, {"preceded_by", "3T"}, {"succeeded_by", "3T"}},
        QJsonObject{{"signal_id", "ST002"}, {"preceded_by", "4T"}, {"succeeded_by", "4T"}},
        QJsonObject{{"signal_id", "ST003"}, {"preceded_by", "3T"}, {"succeeded_by", "3T"}},
        QJsonObject{{"signal_id", "ST004"}, {"preceded_by", "4T"}, {"succeeded_by", "4T"}},
        
        // ADVANCED_STARTER signals
        QJsonObject{{"signal_id", "AS001"}, {"preceded_by", "2T"}, {"succeeded_by", "1T"}},
        QJsonObject{{"signal_id", "AS002"}, {"preceded_by", "1T"}, {"succeeded_by", "2T"}},
        QJsonObject{{"signal_id", "AS003"}, {"preceded_by", "1T"}, {"succeeded_by", "A1T"}},
        QJsonObject{{"signal_id", "AS004"}, {"preceded_by", "A1T"}, {"succeeded_by", "1T"}},
        
        // OUTER signals
        QJsonObject{{"signal_id", "OS001"}, {"preceded_by", "W22T"}, {"succeeded_by", "W22T"}},
        QJsonObject{{"signal_id", "OS002"}, {"preceded_by", "W21T"}, {"succeeded_by", "W21T"}}
    };
    
    for (const QJsonValue& value : anchorMappings) {
        QJsonObject anchor = value.toObject();
        QString signalId = anchor["signal_id"].toString();
        QString precededBy = anchor["preceded_by"].toString();
        QString succeededBy = anchor["succeeded_by"].toString();
        
        if (!executeQuery(
            "UPDATE railway_control.signals SET "
            "preceded_by_circuit_id = ?, "
            "succeeded_by_circuit_id = ? "
            "WHERE signal_id = ?",
            QVariantList{precededBy, succeededBy, signalId}
        )) {
            qWarning() << "Failed to update signal anchors for" << signalId;
            return false;
        }
    }
    
    qDebug() << "✅ Populated" << anchorMappings.size() << "signal adjacency anchors";
    return true;
}

bool DatabaseInitializer::populateTrackCircuitEdges() {
    qDebug() << "🔄 Populating track circuit edges for pathfinding...";
    
    // Track circuit connectivity edges - defines the pathfinding graph
    QJsonArray edgeDefinitions = QJsonArray {
        // Main line connections (unconditional)
        QJsonObject{{"from", "W22T"}, {"to", "23T"}, {"side", "RIGHT"}, {"weight", 1.0}},
        QJsonObject{{"from", "23T"}, {"to", "3T"}, {"side", "RIGHT"}, {"weight", 1.0}},
        QJsonObject{{"from", "3T"}, {"to", "2T"}, {"side", "RIGHT"}, {"weight", 1.0}},
        QJsonObject{{"from", "2T"}, {"to", "1T"}, {"side", "RIGHT"}, {"weight", 1.0}},
        QJsonObject{{"from", "1T"}, {"to", "A1T"}, {"side", "RIGHT"}, {"weight", 1.0}},
        
        // Reverse direction
        QJsonObject{{"from", "A1T"}, {"to", "1T"}, {"side", "LEFT"}, {"weight", 1.0}},
        QJsonObject{{"from", "1T"}, {"to", "2T"}, {"side", "LEFT"}, {"weight", 1.0}},
        QJsonObject{{"from", "2T"}, {"to", "3T"}, {"side", "LEFT"}, {"weight", 1.0}},
        QJsonObject{{"from", "3T"}, {"to", "23T"}, {"side", "LEFT"}, {"weight", 1.0}},
        QJsonObject{{"from", "23T"}, {"to", "W22T"}, {"side", "LEFT"}, {"weight", 1.0}},
        
        // Platform connections via PM001 (conditional)
        QJsonObject{{"from", "W22T"}, {"to", "4T"}, {"side", "RIGHT"}, {"pm", "PM001"}, {"position", "REVERSE"}, {"weight", 1.2}},
        QJsonObject{{"from", "4T"}, {"to", "W21T"}, {"side", "RIGHT"}, {"weight", 1.0}},
        QJsonObject{{"from", "W21T"}, {"to", "2T"}, {"side", "RIGHT"}, {"weight", 1.0}},
        
        // Reverse platform connections
        QJsonObject{{"from", "2T"}, {"to", "W21T"}, {"side", "LEFT"}, {"weight", 1.0}},
        QJsonObject{{"from", "W21T"}, {"to", "4T"}, {"side", "LEFT"}, {"weight", 1.0}},
        QJsonObject{{"from", "4T"}, {"to", "W22T"}, {"side", "LEFT"}, {"pm", "PM001"}, {"position", "REVERSE"}, {"weight", 1.2}},
        
        // Direct connections when PM001 is NORMAL
        QJsonObject{{"from", "W22T"}, {"to", "W21T"}, {"side", "RIGHT"}, {"pm", "PM001"}, {"position", "NORMAL"}, {"weight", 2.0}},
        QJsonObject{{"from", "W21T"}, {"to", "W22T"}, {"side", "LEFT"}, {"pm", "PM001"}, {"position", "NORMAL"}, {"weight", 2.0}}
    };
    
    for (const QJsonValue& value : edgeDefinitions) {
        QJsonObject edge = value.toObject();
        QString fromCircuit = edge["from"].toString();
        QString toCircuit = edge["to"].toString();
        QString side = edge["side"].toString();
        double weight = edge["weight"].toDouble(1.0);
        
        QString pmId;
        QString position;
        if (edge.contains("pm")) {
            pmId = edge["pm"].toString();
            position = edge["position"].toString();
        }
        
        QVariantList params{fromCircuit, toCircuit, side, weight};
        QString query = "INSERT INTO railway_control.track_circuit_edges "
                       "(from_circuit_id, to_circuit_id, side, weight";
        
        if (!pmId.isEmpty()) {
            query += ", condition_point_machine_id, condition_position";
            params << pmId << position;
        }
        
        query += ") VALUES (?, ?, ?, ?";
        if (!pmId.isEmpty()) {
            query += ", ?, ?";
        }
        query += ")";
        
        if (!executeQuery(query, params)) {
            qWarning() << "Failed to insert track circuit edge:" << fromCircuit << "->" << toCircuit;
            return false;
        }
    }
    
    qDebug() << "✅ Populated" << edgeDefinitions.size() << "track circuit edges";
    return true;
}

bool DatabaseInitializer::populateSignalOverlapDefinitions() {
    qDebug() << "🔄 Populating signal overlap definitions...";
    
    // Signal overlap definitions - safety braking distances
    QJsonArray overlapDefinitions = QJsonArray {
        // HOME signals - overlap to next signal
        QJsonObject{
            {"signal_id", "HM001"}, 
            {"overlap_circuits", QJsonArray{"3T"}}, 
            {"release_triggers", QJsonArray{"W22T"}},
            {"hold_seconds", 30}
        },
        QJsonObject{
            {"signal_id", "HM002"}, 
            {"overlap_circuits", QJsonArray{"4T"}}, 
            {"release_triggers", QJsonArray{"W21T"}},
            {"hold_seconds", 30}
        },
        
        // STARTER signals - overlap beyond platform
        QJsonObject{
            {"signal_id", "ST001"}, 
            {"overlap_circuits", QJsonArray{"2T", "1T"}}, 
            {"release_triggers", QJsonArray{"3T"}},
            {"hold_seconds", 25}
        },
        QJsonObject{
            {"signal_id", "ST002"}, 
            {"overlap_circuits", QJsonArray{"W21T", "2T"}}, 
            {"release_triggers", QJsonArray{"4T"}},
            {"hold_seconds", 25}
        },
        QJsonObject{
            {"signal_id", "ST003"}, 
            {"overlap_circuits", QJsonArray{"23T", "W22T"}}, 
            {"release_triggers", QJsonArray{"3T"}},
            {"hold_seconds", 25}
        },
        QJsonObject{
            {"signal_id", "ST004"}, 
            {"overlap_circuits", QJsonArray{"W22T", "23T"}}, 
            {"release_triggers", QJsonArray{"4T"}},
            {"hold_seconds", 25}
        },
        
        // ADVANCED_STARTER signals - final overlap
        QJsonObject{
            {"signal_id", "AS001"}, 
            {"overlap_circuits", QJsonArray{"A1T"}}, 
            {"release_triggers", QJsonArray{"2T"}},
            {"hold_seconds", 20}
        },
        QJsonObject{
            {"signal_id", "AS002"}, 
            {"overlap_circuits", QJsonArray{"2T"}}, 
            {"release_triggers", QJsonArray{"1T"}},
            {"hold_seconds", 20}
        }
    };
    
    for (const QJsonValue& value : overlapDefinitions) {
        QJsonObject overlap = value.toObject();
        QString signalId = overlap["signal_id"].toString();
        QJsonArray overlapCircuits = overlap["overlap_circuits"].toArray();
        QJsonArray releaseTriggers = overlap["release_triggers"].toArray();
        int holdSeconds = overlap["hold_seconds"].toInt(30);
        
        // Convert JSON arrays to PostgreSQL arrays
        QStringList overlapList, triggerList;
        for (const QJsonValue& circuit : overlapCircuits) {
            overlapList << circuit.toString();
        }
        for (const QJsonValue& trigger : releaseTriggers) {
            triggerList << trigger.toString();
        }
        
        QString overlapArray = QString("{%1}").arg(overlapList.join(","));
        QString triggerArray = QString("{%1}").arg(triggerList.join(","));
        
        if (!executeQuery(
            "INSERT INTO railway_control.signal_overlap_definitions "
            "(signal_id, overlap_circuit_ids, release_trigger_circuit_ids, overlap_hold_seconds) "
            "VALUES (?, ?::text[], ?::text[], ?)",
            QVariantList{signalId, overlapArray, triggerArray, holdSeconds}
        )) {
            qWarning() << "Failed to insert overlap definition for" << signalId;
            return false;
        }
    }
    
    qDebug() << "✅ Populated" << overlapDefinitions.size() << "signal overlap definitions";
    return true;
}
