-- ============================================================================
-- RailFlux Railway Control System Database Schema
-- Version: 1.0.1
-- Description: Complete database schema for railway control system
-- PostgreSQL 10+ Compatible - Fixed syntax issues
-- ============================================================================

-- Create schemas for organization
CREATE SCHEMA IF NOT EXISTS railway_control;
CREATE SCHEMA IF NOT EXISTS railway_audit;
CREATE SCHEMA IF NOT EXISTS railway_config;

-- Set search path
SET search_path TO railway_control, railway_audit, railway_config, public;

-- ============================================================================
-- LOOKUP TABLES (Configuration)
-- ============================================================================

CREATE TABLE railway_config.signal_types (
    id SERIAL PRIMARY KEY,
    type_code VARCHAR(20) NOT NULL UNIQUE,
    type_name VARCHAR(50) NOT NULL,
    description TEXT,
    max_aspects INTEGER NOT NULL DEFAULT 2,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE railway_config.signal_aspects (
    id SERIAL PRIMARY KEY,
    aspect_code VARCHAR(20) NOT NULL UNIQUE,
    aspect_name VARCHAR(50) NOT NULL,
    color_code VARCHAR(7) NOT NULL, -- Hex color
    description TEXT,
    safety_level INTEGER NOT NULL DEFAULT 0, -- 0=danger, 1=caution, 2=clear
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE railway_config.point_positions (
    id SERIAL PRIMARY KEY,
    position_code VARCHAR(20) NOT NULL UNIQUE,
    position_name VARCHAR(50) NOT NULL,
    description TEXT,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- ============================================================================
-- CORE RAILWAY INFRASTRUCTURE TABLES
-- ============================================================================

CREATE TABLE railway_control.track_segments (
    id SERIAL PRIMARY KEY,
    segment_id VARCHAR(20) NOT NULL UNIQUE,
    segment_name VARCHAR(100),
    start_row NUMERIC(10,2) NOT NULL,
    start_col NUMERIC(10,2) NOT NULL,
    end_row NUMERIC(10,2) NOT NULL,
    end_col NUMERIC(10,2) NOT NULL,
    track_type VARCHAR(20) DEFAULT 'STRAIGHT',
    is_occupied BOOLEAN DEFAULT FALSE,
    is_assigned BOOLEAN DEFAULT FALSE,
    occupied_by VARCHAR(50), -- Train ID or assignment
    circuit_id VARCHAR(20),
    length_meters NUMERIC(10,2),
    max_speed_kmh INTEGER,
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    -- Constraints
    CONSTRAINT chk_coordinates CHECK (
        start_row >= 0 AND start_col >= 0 AND 
        end_row >= 0 AND end_col >= 0
    )
);

CREATE TABLE railway_control.signals (
    id SERIAL PRIMARY KEY,
    signal_id VARCHAR(20) NOT NULL UNIQUE,
    signal_name VARCHAR(100) NOT NULL,
    signal_type_id INTEGER NOT NULL REFERENCES railway_config.signal_types(id),
    location_row NUMERIC(10,2) NOT NULL,
    location_col NUMERIC(10,2) NOT NULL,
    direction VARCHAR(10) NOT NULL CHECK (direction IN ('UP', 'DOWN')),
    current_aspect_id INTEGER REFERENCES railway_config.signal_aspects(id),
    
    -- Home signal specific fields
    calling_on_aspect VARCHAR(20) DEFAULT 'OFF',
    loop_aspect VARCHAR(20) DEFAULT 'OFF',
    loop_signal_configuration VARCHAR(10) DEFAULT 'UR',
    
    -- Signal properties
    aspect_count INTEGER NOT NULL DEFAULT 2,
    possible_aspects TEXT[], -- Array of possible aspect codes
    is_active BOOLEAN DEFAULT TRUE,
    location_description VARCHAR(200),
    
    -- Operational fields
    last_changed_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    last_changed_by VARCHAR(100),
    interlocked_with INTEGER[], -- Array of signal IDs this is interlocked with
    
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    -- Constraints
    CONSTRAINT chk_location CHECK (location_row >= 0 AND location_col >= 0),
    CONSTRAINT chk_aspect_count CHECK (aspect_count >= 2 AND aspect_count <= 4)
);

CREATE TABLE railway_control.point_machines (
    id SERIAL PRIMARY KEY,
    machine_id VARCHAR(20) NOT NULL UNIQUE,
    machine_name VARCHAR(100) NOT NULL,
    
    -- Junction geometry
    junction_row NUMERIC(10,2) NOT NULL,
    junction_col NUMERIC(10,2) NOT NULL,
    
    -- Track connections (JSON for flexibility)
    root_track_connection JSONB NOT NULL,
    normal_track_connection JSONB NOT NULL,
    reverse_track_connection JSONB NOT NULL,
    
    -- Current state
    current_position_id INTEGER REFERENCES railway_config.point_positions(id),
    operating_status VARCHAR(20) DEFAULT 'CONNECTED' CHECK (
        operating_status IN ('CONNECTED', 'IN_TRANSITION', 'FAILED', 'LOCKED_OUT')
    ),
    
    -- Operational fields
    transition_time_ms INTEGER DEFAULT 3000,
    last_operated_at TIMESTAMP WITH TIME ZONE,
    last_operated_by VARCHAR(100),
    operation_count INTEGER DEFAULT 0,
    
    -- Safety interlocks
    safety_interlocks INTEGER[], -- Array of signal IDs that must be RED before operation
    is_locked BOOLEAN DEFAULT FALSE,
    lock_reason TEXT,
    
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    -- Constraints
    CONSTRAINT chk_junction_location CHECK (junction_row >= 0 AND junction_col >= 0)
);

CREATE TABLE railway_control.text_labels (
    id SERIAL PRIMARY KEY,
    label_text VARCHAR(200) NOT NULL,
    position_row NUMERIC(10,2) NOT NULL,
    position_col NUMERIC(10,2) NOT NULL,
    font_size INTEGER DEFAULT 12,
    color VARCHAR(7) DEFAULT '#ffffff',
    font_family VARCHAR(50) DEFAULT 'Arial',
    is_visible BOOLEAN DEFAULT TRUE,
    label_type VARCHAR(20) DEFAULT 'INFO', -- INFO, WARNING, GRID_REFERENCE
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- ============================================================================
-- AUDIT AND EVENT LOGGING SYSTEM
-- ============================================================================

CREATE TABLE railway_audit.event_log (
    id BIGSERIAL PRIMARY KEY,
    event_timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    event_type VARCHAR(50) NOT NULL,
    entity_type VARCHAR(50) NOT NULL, -- SIGNAL, POINT_MACHINE, TRACK_SEGMENT
    entity_id VARCHAR(50) NOT NULL,
    entity_name VARCHAR(100),
    
    -- Change details
    old_values JSONB,
    new_values JSONB,
    field_changed VARCHAR(100),
    
    -- Context
    operator_id VARCHAR(100),
    operator_name VARCHAR(200),
    operation_source VARCHAR(50) DEFAULT 'HMI', -- HMI, API, AUTOMATIC, SYSTEM
    session_id VARCHAR(100),
    ip_address INET,
    
    -- Safety and compliance
    safety_critical BOOLEAN DEFAULT FALSE,
    authorization_level VARCHAR(20),
    reason_code VARCHAR(50),
    comments TEXT,
    
    -- Replay capability
    replay_data JSONB, -- Complete state for replay
    sequence_number BIGINT,
    
    -- Date for partitioning (computed via trigger instead of generated column)
    event_date DATE
);

-- Trigger to automatically set event_date
CREATE OR REPLACE FUNCTION railway_audit.set_event_date()
RETURNS TRIGGER AS $$
BEGIN
    NEW.event_date := NEW.event_timestamp::DATE;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_event_log_set_date
    BEFORE INSERT OR UPDATE ON railway_audit.event_log
    FOR EACH ROW EXECUTE FUNCTION railway_audit.set_event_date();

CREATE TABLE railway_audit.system_events (
    id BIGSERIAL PRIMARY KEY,
    event_timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    event_level VARCHAR(20) NOT NULL CHECK (event_level IN ('INFO', 'WARNING', 'ERROR', 'CRITICAL')),
    event_category VARCHAR(50) NOT NULL, -- DATABASE, COMMUNICATION, SAFETY, PERFORMANCE
    event_message TEXT NOT NULL,
    event_details JSONB,
    source_component VARCHAR(100),
    error_code VARCHAR(20),
    resolved_at TIMESTAMP WITH TIME ZONE,
    resolved_by VARCHAR(100)
);

-- ============================================================================
-- OPERATIONAL STATE TRACKING
-- ============================================================================

CREATE TABLE railway_control.system_state (
    id SERIAL PRIMARY KEY,
    state_key VARCHAR(100) NOT NULL UNIQUE,
    state_value JSONB NOT NULL,
    description TEXT,
    last_updated TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_by VARCHAR(100)
);

-- ============================================================================
-- INDEXES FOR PERFORMANCE
-- ============================================================================

-- Track segments
CREATE INDEX idx_track_segments_segment_id ON railway_control.track_segments(segment_id);
CREATE INDEX idx_track_segments_occupied ON railway_control.track_segments(is_occupied) WHERE is_occupied = TRUE;
CREATE INDEX idx_track_segments_assigned ON railway_control.track_segments(is_assigned) WHERE is_assigned = TRUE;
CREATE INDEX idx_track_segments_location ON railway_control.track_segments USING btree(start_row, start_col, end_row, end_col);

-- Signals
CREATE INDEX idx_signals_signal_id ON railway_control.signals(signal_id);
CREATE INDEX idx_signals_type ON railway_control.signals(signal_type_id);
CREATE INDEX idx_signals_location ON railway_control.signals USING btree(location_row, location_col);
CREATE INDEX idx_signals_active ON railway_control.signals(is_active) WHERE is_active = TRUE;
CREATE INDEX idx_signals_last_changed ON railway_control.signals(last_changed_at);

-- Point machines
CREATE INDEX idx_point_machines_machine_id ON railway_control.point_machines(machine_id);
CREATE INDEX idx_point_machines_position ON railway_control.point_machines(current_position_id);
CREATE INDEX idx_point_machines_status ON railway_control.point_machines(operating_status);
CREATE INDEX idx_point_machines_junction ON railway_control.point_machines USING btree(junction_row, junction_col);

-- Event log (critical for performance)
CREATE INDEX idx_event_log_timestamp ON railway_audit.event_log(event_timestamp);
CREATE INDEX idx_event_log_entity ON railway_audit.event_log(entity_type, entity_id);
CREATE INDEX idx_event_log_operator ON railway_audit.event_log(operator_id);
CREATE INDEX idx_event_log_safety ON railway_audit.event_log(safety_critical) WHERE safety_critical = TRUE;
CREATE INDEX idx_event_log_sequence ON railway_audit.event_log(sequence_number);
CREATE INDEX idx_event_log_date ON railway_audit.event_log(event_date);

-- GIN indexes for JSONB and array columns
CREATE INDEX idx_signals_possible_aspects ON railway_control.signals USING gin(possible_aspects);
CREATE INDEX idx_signals_interlocked_with ON railway_control.signals USING gin(interlocked_with);
CREATE INDEX idx_point_machines_safety_interlocks ON railway_control.point_machines USING gin(safety_interlocks);
CREATE INDEX idx_event_log_old_values ON railway_audit.event_log USING gin(old_values);
CREATE INDEX idx_event_log_new_values ON railway_audit.event_log USING gin(new_values);
CREATE INDEX idx_event_log_replay_data ON railway_audit.event_log USING gin(replay_data);

-- ============================================================================
-- TRIGGERS FOR AUTOMATIC TIMESTAMP UPDATES
-- ============================================================================

-- Generic trigger function for updated_at
CREATE OR REPLACE FUNCTION railway_control.update_timestamp()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Apply to all main tables
CREATE TRIGGER trg_track_segments_updated_at
    BEFORE UPDATE ON railway_control.track_segments
    FOR EACH ROW EXECUTE FUNCTION railway_control.update_timestamp();

CREATE TRIGGER trg_signals_updated_at
    BEFORE UPDATE ON railway_control.signals
    FOR EACH ROW EXECUTE FUNCTION railway_control.update_timestamp();

CREATE TRIGGER trg_point_machines_updated_at
    BEFORE UPDATE ON railway_control.point_machines
    FOR EACH ROW EXECUTE FUNCTION railway_control.update_timestamp();

CREATE TRIGGER trg_text_labels_updated_at
    BEFORE UPDATE ON railway_control.text_labels
    FOR EACH ROW EXECUTE FUNCTION railway_control.update_timestamp();

-- Update signal last_changed_at when aspect changes
CREATE OR REPLACE FUNCTION railway_control.update_signal_change_time()
RETURNS TRIGGER AS $$
BEGIN
    IF OLD.current_aspect_id IS DISTINCT FROM NEW.current_aspect_id THEN
        NEW.last_changed_at = CURRENT_TIMESTAMP;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_signals_aspect_changed
    BEFORE UPDATE ON railway_control.signals
    FOR EACH ROW EXECUTE FUNCTION railway_control.update_signal_change_time();

-- ============================================================================
-- AUDIT LOGGING TRIGGERS
-- ============================================================================

-- Create sequence for event ordering
CREATE SEQUENCE railway_audit.event_sequence;

-- Generic audit logging function
CREATE OR REPLACE FUNCTION railway_audit.log_changes()
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
            ELSE false 
        END,
        COALESCE(new_json, old_json),
        nextval('railway_audit.event_sequence')
    );

    RETURN COALESCE(NEW, OLD);
END;
$$ LANGUAGE plpgsql;

-- Apply audit triggers to critical tables
CREATE TRIGGER trg_track_segments_audit
    AFTER INSERT OR UPDATE OR DELETE ON railway_control.track_segments
    FOR EACH ROW EXECUTE FUNCTION railway_audit.log_changes();

CREATE TRIGGER trg_signals_audit
    AFTER INSERT OR UPDATE OR DELETE ON railway_control.signals
    FOR EACH ROW EXECUTE FUNCTION railway_audit.log_changes();

CREATE TRIGGER trg_point_machines_audit
    AFTER INSERT OR UPDATE OR DELETE ON railway_control.point_machines
    FOR EACH ROW EXECUTE FUNCTION railway_audit.log_changes();

-- ============================================================================
-- REAL-TIME NOTIFICATION SYSTEM - FIXED VERSION
-- ============================================================================

-- Track segments notification function
CREATE OR REPLACE FUNCTION railway_control.notify_track_changes()
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
$$ LANGUAGE plpgsql;

-- Signals notification function
CREATE OR REPLACE FUNCTION railway_control.notify_signal_changes()
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
$$ LANGUAGE plpgsql;

-- Point machines notification function
CREATE OR REPLACE FUNCTION railway_control.notify_point_changes()
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
$$ LANGUAGE plpgsql;

-- Apply notification triggers with specific functions
CREATE TRIGGER trg_track_segments_notify
    AFTER INSERT OR UPDATE OR DELETE ON railway_control.track_segments
    FOR EACH ROW EXECUTE FUNCTION railway_control.notify_track_changes();

CREATE TRIGGER trg_signals_notify
    AFTER INSERT OR UPDATE OR DELETE ON railway_control.signals
    FOR EACH ROW EXECUTE FUNCTION railway_control.notify_signal_changes();

CREATE TRIGGER trg_point_machines_notify
    AFTER INSERT OR UPDATE OR DELETE ON railway_control.point_machines
    FOR EACH ROW EXECUTE FUNCTION railway_control.notify_point_changes();

-- ============================================================================
-- VIEWS FOR COMMON QUERIES
-- ============================================================================

-- Complete signal information view
CREATE VIEW railway_control.v_signals_complete AS
SELECT 
    s.id,
    s.signal_id,
    s.signal_name,
    st.type_code as signal_type,
    st.type_name as signal_type_name,
    s.location_row,
    s.location_col,
    s.direction,
    sa.aspect_code as current_aspect,
    sa.aspect_name as current_aspect_name,
    sa.color_code as current_aspect_color,
    s.calling_on_aspect,
    s.loop_aspect,
    s.loop_signal_configuration,
    s.aspect_count,
    s.possible_aspects,
    s.is_active,
    s.location_description,
    s.last_changed_at,
    s.last_changed_by,
    s.created_at,
    s.updated_at
FROM railway_control.signals s
JOIN railway_config.signal_types st ON s.signal_type_id = st.id
LEFT JOIN railway_config.signal_aspects sa ON s.current_aspect_id = sa.id;

-- Complete point machine information view
CREATE VIEW railway_control.v_point_machines_complete AS
SELECT 
    pm.id,
    pm.machine_id,
    pm.machine_name,
    pm.junction_row,
    pm.junction_col,
    pm.root_track_connection,
    pm.normal_track_connection,
    pm.reverse_track_connection,
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
LEFT JOIN railway_config.point_positions pp ON pm.current_position_id = pp.id;

-- Track occupancy summary
CREATE VIEW railway_control.v_track_occupancy AS
SELECT 
    COUNT(*) as total_segments,
    COUNT(*) FILTER (WHERE is_occupied) as occupied_count,
    COUNT(*) FILTER (WHERE is_assigned) as assigned_count,
    COUNT(*) FILTER (WHERE is_occupied OR is_assigned) as unavailable_count,
    ROUND(
        (COUNT(*) FILTER (WHERE is_occupied OR is_assigned)::NUMERIC / COUNT(*)) * 100, 
        2
    ) as utilization_percentage
FROM railway_control.track_segments
WHERE is_active = TRUE;

-- Recent events view
CREATE VIEW railway_audit.v_recent_events AS
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
ORDER BY el.event_timestamp DESC;

-- ============================================================================
-- HELPER FUNCTIONS
-- ============================================================================

-- Function to get signal aspect by code
CREATE OR REPLACE FUNCTION railway_config.get_aspect_id(aspect_code_param VARCHAR)
RETURNS INTEGER AS $$
DECLARE
    aspect_id_result INTEGER;
BEGIN
    SELECT id INTO aspect_id_result 
    FROM railway_config.signal_aspects 
    WHERE aspect_code = aspect_code_param;
    
    RETURN aspect_id_result;
END;
$$ LANGUAGE plpgsql;

-- Function to get position ID by code
CREATE OR REPLACE FUNCTION railway_config.get_position_id(position_code_param VARCHAR)
RETURNS INTEGER AS $$
DECLARE
    position_id_result INTEGER;
BEGIN
    SELECT id INTO position_id_result 
    FROM railway_config.point_positions 
    WHERE position_code = position_code_param;
    
    RETURN position_id_result;
END;
$$ LANGUAGE plpgsql;

-- Function to safely update signal aspect with validation - FIXED
CREATE OR REPLACE FUNCTION railway_control.update_signal_aspect(
    signal_id_param VARCHAR,
    aspect_code_param VARCHAR,
    operator_id_param VARCHAR DEFAULT 'system'
)
RETURNS BOOLEAN AS $$
DECLARE
    aspect_id_val INTEGER;
    rows_affected INTEGER;
BEGIN
    -- Set operator context for audit logging
    PERFORM set_config('railway.operator_id', operator_id_param, true);
    
    -- Get aspect ID
    aspect_id_val := railway_config.get_aspect_id(aspect_code_param);
    IF aspect_id_val IS NULL THEN
        RAISE EXCEPTION 'Invalid aspect code: %', aspect_code_param;
    END IF;
    
    -- Check if signal exists and update
    UPDATE railway_control.signals 
    SET current_aspect_id = aspect_id_val
    WHERE signal_id = signal_id_param;
    
    -- ✅ FIXED: Use GET DIAGNOSTICS with ROW_COUNT instead of FOUND
    GET DIAGNOSTICS rows_affected = ROW_COUNT;
    RETURN rows_affected > 0;
END;
$$ LANGUAGE plpgsql;

-- Function to safely update point machine position - FIXED
CREATE OR REPLACE FUNCTION railway_control.update_point_position(
    machine_id_param VARCHAR,
    position_code_param VARCHAR,
    operator_id_param VARCHAR DEFAULT 'system'
)
RETURNS BOOLEAN AS $$
DECLARE
    position_id_val INTEGER;
    rows_affected INTEGER;
BEGIN
    -- Set operator context for audit logging
    PERFORM set_config('railway.operator_id', operator_id_param, true);
    
    -- Get position ID
    position_id_val := railway_config.get_position_id(position_code_param);
    IF position_id_val IS NULL THEN
        RAISE EXCEPTION 'Invalid position code: %', position_code_param;
    END IF;
    
    -- Update point machine position and increment operation count
    UPDATE railway_control.point_machines 
    SET 
        current_position_id = position_id_val,
        last_operated_at = CURRENT_TIMESTAMP,
        last_operated_by = operator_id_param,
        operation_count = operation_count + 1
    WHERE machine_id = machine_id_param;
    
    -- ✅ FIXED: Use GET DIAGNOSTICS with ROW_COUNT instead of FOUND
    GET DIAGNOSTICS rows_affected = ROW_COUNT;
    RETURN rows_affected > 0;
END;
$$ LANGUAGE plpgsql;

-- Function to update track occupancy with audit logging
CREATE OR REPLACE FUNCTION railway_control.update_track_occupancy(
    segment_id_param VARCHAR,
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
    
    -- Update track segment occupancy
    UPDATE railway_control.track_segments 
    SET 
        is_occupied = is_occupied_param,
        occupied_by = CASE 
            WHEN is_occupied_param = TRUE THEN occupied_by_param 
            ELSE NULL 
        END
    WHERE segment_id = segment_id_param;
    
    GET DIAGNOSTICS rows_affected = ROW_COUNT;
    RETURN rows_affected > 0;
END;
$$ LANGUAGE plpgsql;

-- Function to update track assignment with audit logging
CREATE OR REPLACE FUNCTION railway_control.update_track_assignment(
    segment_id_param VARCHAR,
    is_assigned_param BOOLEAN,
    operator_id_param VARCHAR DEFAULT 'system'
)
RETURNS BOOLEAN AS $$
DECLARE
    rows_affected INTEGER;
BEGIN
    -- Set operator context for audit logging
    PERFORM set_config('railway.operator_id', operator_id_param, true);
    
    -- Update track segment assignment
    UPDATE railway_control.track_segments 
    SET is_assigned = is_assigned_param
    WHERE segment_id = segment_id_param;
    
    GET DIAGNOSTICS rows_affected = ROW_COUNT;
    RETURN rows_affected > 0;
END;
$$ LANGUAGE plpgsql;

-- Function to get current system status
CREATE OR REPLACE FUNCTION railway_control.get_system_status()
RETURNS JSON AS $$
DECLARE
    result JSON;
    track_stats RECORD;
    signal_stats RECORD;
    point_stats RECORD;
BEGIN
    -- Get track statistics
    SELECT 
        COUNT(*) as total,
        COUNT(*) FILTER (WHERE is_occupied) as occupied,
        COUNT(*) FILTER (WHERE is_assigned) as assigned
    INTO track_stats
    FROM railway_control.track_segments
    WHERE is_active = TRUE;
    
    -- Get signal statistics
    SELECT 
        COUNT(*) as total,
        COUNT(*) FILTER (WHERE is_active) as active
    INTO signal_stats
    FROM railway_control.signals;
    
    -- Get point machine statistics
    SELECT 
        COUNT(*) as total,
        COUNT(*) FILTER (WHERE operating_status = 'CONNECTED') as connected,
        COUNT(*) FILTER (WHERE operating_status = 'IN_TRANSITION') as in_transition
    INTO point_stats
    FROM railway_control.point_machines;
    
    -- Build result JSON
    result := json_build_object(
        'timestamp', extract(epoch from now()),
        'tracks', json_build_object(
            'total', track_stats.total,
            'occupied', track_stats.occupied,
            'assigned', track_stats.assigned,
            'available', track_stats.total - track_stats.occupied - track_stats.assigned
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
$$ LANGUAGE plpgsql;

-- ============================================================================
-- SECURITY: Create roles and permissions
-- ============================================================================

-- Railway Control Operator (full access to operations)
CREATE ROLE railway_operator;
GRANT USAGE ON SCHEMA railway_control TO railway_operator;
GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA railway_control TO railway_operator;
GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA railway_control TO railway_operator;
GRANT USAGE ON SCHEMA railway_config TO railway_operator;
GRANT SELECT ON ALL TABLES IN SCHEMA railway_config TO railway_operator;

-- Railway Observer (read-only access)
CREATE ROLE railway_observer;
GRANT USAGE ON SCHEMA railway_control TO railway_observer;
GRANT SELECT ON ALL TABLES IN SCHEMA railway_control TO railway_observer;
GRANT USAGE ON SCHEMA railway_config TO railway_observer;
GRANT SELECT ON ALL TABLES IN SCHEMA railway_config TO railway_observer;

-- Railway Auditor (access to audit logs)
CREATE ROLE railway_auditor;
GRANT USAGE ON SCHEMA railway_audit TO railway_auditor;
GRANT SELECT ON ALL TABLES IN SCHEMA railway_audit TO railway_auditor;

COMMENT ON SCHEMA railway_control IS 'Main railway control system operational data';
COMMENT ON SCHEMA railway_audit IS 'Audit trail and event logging for compliance';
COMMENT ON SCHEMA railway_config IS 'Configuration and lookup tables';