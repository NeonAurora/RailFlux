-- ============================================================================
-- RailFlux Route Assignment System - Database Schema Extensions v2.0
-- CRITICAL: Execute in maintenance window only
-- Extends existing sql_coomands_railflux.sql with route assignment capabilities
-- ============================================================================

BEGIN;

-- ============================================================================
-- 1. SIGNAL ADJACENCY ENHANCEMENT
-- ============================================================================

-- Add pathfinding anchor fields to existing signals table
ALTER TABLE railway_control.signals 
ADD COLUMN IF NOT EXISTS preceded_by_circuit_id TEXT,
ADD COLUMN IF NOT EXISTS succeeded_by_circuit_id TEXT;

-- Add comments for clarity
COMMENT ON COLUMN railway_control.signals.preceded_by_circuit_id IS 'Track circuit behind the signal (approach side) for pathfinding';
COMMENT ON COLUMN railway_control.signals.succeeded_by_circuit_id IS 'Track circuit ahead of the signal (departure side) for pathfinding';

-- Create indexes for pathfinding performance
CREATE INDEX IF NOT EXISTS idx_signals_preceded_by ON railway_control.signals(preceded_by_circuit_id) WHERE preceded_by_circuit_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_signals_succeeded_by ON railway_control.signals(succeeded_by_circuit_id) WHERE succeeded_by_circuit_id IS NOT NULL;

-- ============================================================================
-- 2. TRACK CIRCUIT EDGES - NORMALIZED PATHFINDING GRAPH
-- ============================================================================

CREATE TABLE IF NOT EXISTS railway_control.track_circuit_edges (
    id SERIAL PRIMARY KEY,
    from_circuit_id TEXT NOT NULL REFERENCES railway_control.track_circuits(circuit_id),
    to_circuit_id TEXT NOT NULL REFERENCES railway_control.track_circuits(circuit_id),
    side TEXT NOT NULL CHECK (side IN ('LEFT', 'RIGHT')),
    condition_point_machine_id TEXT REFERENCES railway_control.point_machines(machine_id),
    condition_position TEXT CHECK (condition_position IN ('NORMAL', 'REVERSE')),
    weight NUMERIC(10,2) DEFAULT 1.0,
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    
    -- Unique constraint ensures no duplicate edges
    UNIQUE(from_circuit_id, to_circuit_id, side, condition_point_machine_id, condition_position)
);

-- Indexes for pathfinding performance
CREATE INDEX idx_track_circuit_edges_from ON railway_control.track_circuit_edges(from_circuit_id) WHERE is_active = TRUE;
CREATE INDEX idx_track_circuit_edges_to ON railway_control.track_circuit_edges(to_circuit_id) WHERE is_active = TRUE;
CREATE INDEX idx_track_circuit_edges_pm ON railway_control.track_circuit_edges(condition_point_machine_id) WHERE condition_point_machine_id IS NOT NULL;
CREATE INDEX idx_track_circuit_edges_active ON railway_control.track_circuit_edges(is_active) WHERE is_active = TRUE;

-- Comments
COMMENT ON TABLE railway_control.track_circuit_edges IS 'Normalized pathfinding graph with conditional edges based on point machine positions';
COMMENT ON COLUMN railway_control.track_circuit_edges.side IS 'Topological side: LEFT for DOWN direction, RIGHT for UP direction';
COMMENT ON COLUMN railway_control.track_circuit_edges.condition_point_machine_id IS 'Point machine that must be in specific position for this edge to be valid';
COMMENT ON COLUMN railway_control.track_circuit_edges.weight IS 'Pathfinding weight for A* algorithm optimization';

-- ============================================================================
-- 3. SIGNAL OVERLAP DEFINITIONS
-- ============================================================================

CREATE TABLE IF NOT EXISTS railway_control.signal_overlap_definitions (
    id SERIAL PRIMARY KEY,
    signal_id TEXT NOT NULL UNIQUE REFERENCES railway_control.signals(signal_id),
    overlap_circuit_ids TEXT[] NOT NULL,
    release_trigger_circuit_ids TEXT[] NOT NULL,
    overlap_type TEXT NOT NULL DEFAULT 'FIXED' CHECK (overlap_type IN ('FIXED', 'VARIABLE', 'FLANK_PROTECTION')),
    overlap_hold_seconds INTEGER NOT NULL DEFAULT 30,
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Indexes
CREATE INDEX idx_signal_overlap_signal ON railway_control.signal_overlap_definitions(signal_id);
CREATE INDEX idx_signal_overlap_active ON railway_control.signal_overlap_definitions(is_active) WHERE is_active = TRUE;
CREATE INDEX idx_signal_overlap_circuits ON railway_control.signal_overlap_definitions USING gin(overlap_circuit_ids);
CREATE INDEX idx_signal_overlap_triggers ON railway_control.signal_overlap_definitions USING gin(release_trigger_circuit_ids);

-- Comments
COMMENT ON TABLE railway_control.signal_overlap_definitions IS 'Signal overlap regions and release trigger definitions';
COMMENT ON COLUMN railway_control.signal_overlap_definitions.overlap_circuit_ids IS 'Circuits reserved beyond destination until timed release';
COMMENT ON COLUMN railway_control.signal_overlap_definitions.release_trigger_circuit_ids IS 'Circuits whose clear sequence indicates main route is released';

-- ============================================================================
-- 4. ROUTE ASSIGNMENTS - MAIN ROUTE STATE TRACKING
-- ============================================================================

CREATE TABLE IF NOT EXISTS railway_control.route_assignments (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    source_signal_id TEXT NOT NULL REFERENCES railway_control.signals(signal_id),
    dest_signal_id TEXT NOT NULL REFERENCES railway_control.signals(signal_id),
    direction TEXT NOT NULL CHECK (direction IN ('UP', 'DOWN')),
    assigned_circuits TEXT[] NOT NULL,
    overlap_circuits TEXT[] NOT NULL DEFAULT '{}',
    state TEXT NOT NULL CHECK (state IN (
        'REQUESTED', 'VALIDATING', 'RESERVED', 'ACTIVE', 
        'PARTIALLY_RELEASED', 'RELEASED', 'FAILED', 
        'EMERGENCY_RELEASED', 'DEGRADED'
    )),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    activated_at TIMESTAMP WITH TIME ZONE,
    released_at TIMESTAMP WITH TIME ZONE,
    overlap_release_due_at TIMESTAMP WITH TIME ZONE,
    locked_point_machines TEXT[] DEFAULT '{}',
    priority INTEGER DEFAULT 100,
    operator_id TEXT NOT NULL DEFAULT 'system',
    failure_reason TEXT,
    performance_metrics JSONB DEFAULT '{}',
    
    -- Constraints
    CONSTRAINT chk_route_timing CHECK (
        (activated_at IS NULL OR activated_at >= created_at) AND
        (released_at IS NULL OR released_at >= created_at) AND
        (overlap_release_due_at IS NULL OR overlap_release_due_at >= created_at)
    ),
    CONSTRAINT chk_route_circuits CHECK (
        array_length(assigned_circuits, 1) > 0
    ),
    CONSTRAINT chk_route_signals CHECK (
        source_signal_id != dest_signal_id
    )
);

-- Indexes for route management performance
CREATE INDEX idx_route_assignments_state ON railway_control.route_assignments(state);
CREATE INDEX idx_route_assignments_source ON railway_control.route_assignments(source_signal_id);
CREATE INDEX idx_route_assignments_dest ON railway_control.route_assignments(dest_signal_id);
CREATE INDEX idx_route_assignments_created ON railway_control.route_assignments(created_at);
CREATE INDEX idx_route_assignments_active ON railway_control.route_assignments(state) WHERE state IN ('RESERVED', 'ACTIVE', 'PARTIALLY_RELEASED');
CREATE INDEX idx_route_assignments_circuits ON railway_control.route_assignments USING gin(assigned_circuits);
CREATE INDEX idx_route_assignments_overlap ON railway_control.route_assignments USING gin(overlap_circuits);
CREATE INDEX idx_route_assignments_pm_locks ON railway_control.route_assignments USING gin(locked_point_machines);
CREATE INDEX idx_route_assignments_overlap_due ON railway_control.route_assignments(overlap_release_due_at) WHERE overlap_release_due_at IS NOT NULL;

-- Comments
COMMENT ON TABLE railway_control.route_assignments IS 'Main route state tracking with complete lifecycle management';
COMMENT ON COLUMN railway_control.route_assignments.performance_metrics IS 'JSON metrics: validation_time_ms, reservation_time_ms, pathfinding_time_ms';

-- ============================================================================
-- 5. ROUTE EVENTS - EVENT SOURCING FOR ROUTE LIFECYCLE
-- ============================================================================

CREATE TABLE IF NOT EXISTS railway_control.route_events (
    id BIGSERIAL PRIMARY KEY,
    route_id UUID NOT NULL REFERENCES railway_control.route_assignments(id),
    event_type TEXT NOT NULL CHECK (event_type IN (
        'ROUTE_REQUESTED', 'VALIDATION_STARTED', 'VALIDATION_COMPLETED',
        'PATHFINDING_COMPLETED', 'RESOURCE_LOCKED', 'ROUTE_RESERVED',
        'POINT_MACHINE_MOVED', 'TRACK_CIRCUIT_OCCUPIED', 'ROUTE_ACTIVATED',
        'MAIN_ROUTE_CLEARED', 'OVERLAP_TIMER_STARTED', 'OVERLAP_RELEASED',
        'ROUTE_RELEASED', 'ROUTE_FAILED', 'EMERGENCY_RELEASE',
        'PERFORMANCE_WARNING', 'SAFETY_VIOLATION'
    )),
    event_timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    event_data JSONB NOT NULL DEFAULT '{}',
    operator_id TEXT,
    source_component TEXT,
    correlation_id UUID,
    sequence_number BIGINT DEFAULT nextval('railway_audit.event_sequence'),
    
    -- Performance and safety metrics
    response_time_ms NUMERIC(10,3),
    safety_critical BOOLEAN DEFAULT FALSE
);

-- Indexes for event sourcing performance
CREATE INDEX idx_route_events_route_id ON railway_control.route_events(route_id);
CREATE INDEX idx_route_events_type ON railway_control.route_events(event_type);
CREATE INDEX idx_route_events_timestamp ON railway_control.route_events(event_timestamp);
CREATE INDEX idx_route_events_sequence ON railway_control.route_events(sequence_number);
CREATE INDEX idx_route_events_correlation ON railway_control.route_events(correlation_id) WHERE correlation_id IS NOT NULL;
CREATE INDEX idx_route_events_safety ON railway_control.route_events(safety_critical) WHERE safety_critical = TRUE;
CREATE INDEX idx_route_events_data ON railway_control.route_events USING gin(event_data);

-- Comments
COMMENT ON TABLE railway_control.route_events IS 'Complete event sourcing for route lifecycle and audit trail';
COMMENT ON COLUMN railway_control.route_events.correlation_id IS 'Links related events across route operations';

-- ============================================================================
-- 6. RESOURCE LOCKS - RESOURCE RESERVATION AND CONFLICT MANAGEMENT
-- ============================================================================

CREATE TABLE IF NOT EXISTS railway_control.resource_locks (
    id SERIAL PRIMARY KEY,
    resource_type TEXT NOT NULL CHECK (resource_type IN ('TRACK_CIRCUIT', 'POINT_MACHINE', 'SIGNAL')),
    resource_id TEXT NOT NULL,
    route_id UUID NOT NULL REFERENCES railway_control.route_assignments(id),
    lock_type TEXT NOT NULL CHECK (lock_type IN ('EXCLUSIVE', 'SHARED', 'OVERLAP')),
    locked_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP WITH TIME ZONE,
    operator_id TEXT NOT NULL,
    lock_reason TEXT,
    is_active BOOLEAN DEFAULT TRUE,
    
    -- Unique constraint prevents double-locking
    UNIQUE(resource_type, resource_id, route_id) DEFERRABLE INITIALLY DEFERRED
);

-- Indexes for lock management performance
CREATE INDEX idx_resource_locks_resource ON railway_control.resource_locks(resource_type, resource_id);
CREATE INDEX idx_resource_locks_route ON railway_control.resource_locks(route_id);
CREATE INDEX idx_resource_locks_active ON railway_control.resource_locks(is_active) WHERE is_active = TRUE;
CREATE INDEX idx_resource_locks_expires ON railway_control.resource_locks(expires_at) WHERE expires_at IS NOT NULL;
CREATE INDEX idx_resource_locks_type ON railway_control.resource_locks(lock_type);

-- Comments
COMMENT ON TABLE railway_control.resource_locks IS 'Resource reservation and conflict management with timeout protection';
COMMENT ON COLUMN railway_control.resource_locks.expires_at IS 'Automatic lock expiration for deadlock prevention';

-- ============================================================================
-- 7. ROUTE CONFIGURATION - SYSTEM CONFIGURATION WITH VITAL/NON-VITAL SETTINGS
-- ============================================================================

CREATE TABLE IF NOT EXISTS railway_control.route_configuration (
    id SERIAL PRIMARY KEY,
    config_key TEXT NOT NULL UNIQUE,
    config_value JSONB NOT NULL,
    config_type TEXT NOT NULL CHECK (config_type IN ('VITAL', 'OPERATIONAL', 'PERFORMANCE')),
    description TEXT,
    default_value JSONB,
    validation_schema JSONB,
    last_updated TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_by TEXT NOT NULL,
    requires_authorization BOOLEAN DEFAULT FALSE,
    
    -- Vital settings audit trail
    change_authorization_id TEXT,
    change_reason TEXT
);

-- Indexes
CREATE INDEX idx_route_config_key ON railway_control.route_configuration(config_key);
CREATE INDEX idx_route_config_type ON railway_control.route_configuration(config_type);
CREATE INDEX idx_route_config_vital ON railway_control.route_configuration(config_type) WHERE config_type = 'VITAL';
CREATE INDEX idx_route_config_updated ON railway_control.route_configuration(last_updated);

-- Comments
COMMENT ON TABLE railway_control.route_configuration IS 'System configuration with vital (safety-critical) vs operational settings';
COMMENT ON COLUMN railway_control.route_configuration.config_type IS 'VITAL changes require special authorization';

-- ============================================================================
-- 8. TRIGGERS FOR AUTOMATIC TIMESTAMP UPDATES
-- ============================================================================

-- Create trigger function for updated_at if not exists
CREATE OR REPLACE FUNCTION railway_control.update_timestamp()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Apply timestamp triggers to new tables
CREATE TRIGGER trg_track_circuit_edges_updated_at
    BEFORE UPDATE ON railway_control.track_circuit_edges
    FOR EACH ROW EXECUTE FUNCTION railway_control.update_timestamp();

CREATE TRIGGER trg_signal_overlap_definitions_updated_at
    BEFORE UPDATE ON railway_control.signal_overlap_definitions
    FOR EACH ROW EXECUTE FUNCTION railway_control.update_timestamp();

-- ============================================================================
-- 9. ROUTE-SPECIFIC NOTIFICATION FUNCTIONS
-- ============================================================================

-- Route assignment changes notification
CREATE OR REPLACE FUNCTION railway_control.notify_route_changes()
RETURNS TRIGGER AS $$
DECLARE
    payload JSON;
BEGIN
    payload := json_build_object(
        'table', 'route_assignments',
        'operation', TG_OP,
        'route_id', COALESCE(NEW.id, OLD.id),
        'state', COALESCE(NEW.state, OLD.state),
        'source_signal_id', COALESCE(NEW.source_signal_id, OLD.source_signal_id),
        'dest_signal_id', COALESCE(NEW.dest_signal_id, OLD.dest_signal_id),
        'timestamp', extract(epoch from now())
    );

    PERFORM pg_notify('route_changes', payload::TEXT);
    RETURN COALESCE(NEW, OLD);
END;
$$ LANGUAGE plpgsql;

-- Resource lock changes notification
CREATE OR REPLACE FUNCTION railway_control.notify_resource_lock_changes()
RETURNS TRIGGER AS $$
DECLARE
    payload JSON;
BEGIN
    payload := json_build_object(
        'table', 'resource_locks',
        'operation', TG_OP,
        'resource_type', COALESCE(NEW.resource_type, OLD.resource_type),
        'resource_id', COALESCE(NEW.resource_id, OLD.resource_id),
        'route_id', COALESCE(NEW.route_id, OLD.route_id),
        'is_active', COALESCE(NEW.is_active, OLD.is_active),
        'timestamp', extract(epoch from now())
    );

    PERFORM pg_notify('resource_lock_changes', payload::TEXT);
    RETURN COALESCE(NEW, OLD);
END;
$$ LANGUAGE plpgsql;

-- Apply notification triggers
CREATE TRIGGER trg_route_assignments_notify
    AFTER INSERT OR UPDATE OR DELETE ON railway_control.route_assignments
    FOR EACH ROW EXECUTE FUNCTION railway_control.notify_route_changes();

CREATE TRIGGER trg_resource_locks_notify
    AFTER INSERT OR UPDATE OR DELETE ON railway_control.resource_locks
    FOR EACH ROW EXECUTE FUNCTION railway_control.notify_resource_lock_changes();

-- ============================================================================
-- 10. ROUTE MANAGEMENT FUNCTIONS
-- ============================================================================

-- Function to get available circuits for route assignment
CREATE OR REPLACE FUNCTION railway_control.get_available_circuits()
RETURNS TABLE(circuit_id TEXT, is_occupied BOOLEAN, is_locked BOOLEAN) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        tc.circuit_id,
        tc.is_occupied,
        EXISTS(
            SELECT 1 FROM railway_control.resource_locks rl 
            WHERE rl.resource_type = 'TRACK_CIRCUIT' 
            AND rl.resource_id = tc.circuit_id 
            AND rl.is_active = TRUE
        ) as is_locked
    FROM railway_control.track_circuits tc
    WHERE tc.is_active = TRUE;
END;
$$ LANGUAGE plpgsql;

-- Function to check point machine availability
CREATE OR REPLACE FUNCTION railway_control.is_point_machine_available(
    machine_id_param TEXT
)
RETURNS BOOLEAN AS $$
DECLARE
    is_locked BOOLEAN;
    is_in_transition BOOLEAN;
BEGIN
    SELECT 
        pm.is_locked OR EXISTS(
            SELECT 1 FROM railway_control.resource_locks rl 
            WHERE rl.resource_type = 'POINT_MACHINE' 
            AND rl.resource_id = machine_id_param 
            AND rl.is_active = TRUE
        ),
        pm.operating_status = 'IN_TRANSITION'
    INTO is_locked, is_in_transition
    FROM railway_control.point_machines pm
    WHERE pm.machine_id = machine_id_param;
    
    RETURN NOT (COALESCE(is_locked, TRUE) OR COALESCE(is_in_transition, TRUE));
END;
$$ LANGUAGE plpgsql;

-- Function to get route pathfinding neighbors
CREATE OR REPLACE FUNCTION railway_control.get_pathfinding_neighbors(
    circuit_id_param TEXT,
    direction_param TEXT,
    point_machine_states JSONB DEFAULT '{}'
)
RETURNS TABLE(neighbor_circuit_id TEXT, weight NUMERIC) AS $$
DECLARE
    side_filter TEXT;
BEGIN
    -- Determine side based on direction
    side_filter := CASE 
        WHEN direction_param = 'UP' THEN 'RIGHT'
        WHEN direction_param = 'DOWN' THEN 'LEFT'
        ELSE 'RIGHT' -- Default fallback
    END;
    
    RETURN QUERY
    SELECT 
        tce.to_circuit_id,
        tce.weight
    FROM railway_control.track_circuit_edges tce
    WHERE tce.from_circuit_id = circuit_id_param
    AND tce.side = side_filter
    AND tce.is_active = TRUE
    AND (
        -- Unconditional edge
        tce.condition_point_machine_id IS NULL
        OR
        -- Conditional edge with PM in required position
        (
            tce.condition_point_machine_id IS NOT NULL
            AND point_machine_states ? tce.condition_point_machine_id
            AND (point_machine_states ->> tce.condition_point_machine_id) = tce.condition_position
        )
    );
END;
$$ LANGUAGE plpgsql;

-- ============================================================================
-- 11. INITIAL CONFIGURATION DATA
-- ============================================================================

-- Insert default route configuration
INSERT INTO railway_control.route_configuration (config_key, config_value, config_type, description, updated_by) VALUES
('max_concurrent_routes', '10', 'VITAL', 'Maximum number of concurrent routes allowed', 'system'),
('pathfinding_timeout_ms', '500', 'PERFORMANCE', 'Maximum time allowed for pathfinding algorithm', 'system'),
('overlap_hold_default_seconds', '30', 'VITAL', 'Default overlap hold time in seconds', 'system'),
('route_validation_timeout_ms', '50', 'VITAL', 'Maximum time allowed for route validation', 'system'),
('resource_lock_timeout_minutes', '30', 'OPERATIONAL', 'Default timeout for resource locks', 'system'),
('degraded_mode_max_routes', '5', 'VITAL', 'Maximum routes in degraded mode', 'system'),
('performance_warning_threshold_ms', '45', 'PERFORMANCE', 'Performance warning threshold', 'system'),
('enable_route_visualization', 'true', 'OPERATIONAL', 'Enable route visualization on UI', 'system'),
('enable_performance_monitoring', 'true', 'OPERATIONAL', 'Enable performance metrics collection', 'system'),
('safety_violation_max_rate', '0.05', 'VITAL', 'Maximum allowed safety violation rate (5%)', 'system')
ON CONFLICT (config_key) DO NOTHING;

-- ============================================================================
-- 12. VIEWS FOR ROUTE MANAGEMENT
-- ============================================================================

-- Complete route information view
CREATE OR REPLACE VIEW railway_control.v_routes_complete AS
SELECT
    ra.id,
    ra.source_signal_id,
    ra.dest_signal_id,
    ra.direction,
    ra.state,
    ra.assigned_circuits,
    ra.overlap_circuits,
    ra.locked_point_machines,
    ra.created_at,
    ra.activated_at,
    ra.released_at,
    ra.overlap_release_due_at,
    ra.priority,
    ra.operator_id,
    ra.failure_reason,
    ra.performance_metrics,
    
    -- Signal information
    src_sig.signal_name as source_signal_name,
    dest_sig.signal_name as dest_signal_name,
    
    -- Timing calculations
    EXTRACT(EPOCH FROM (COALESCE(ra.activated_at, CURRENT_TIMESTAMP) - ra.created_at)) * 1000 as setup_time_ms,
    EXTRACT(EPOCH FROM (COALESCE(ra.released_at, CURRENT_TIMESTAMP) - ra.created_at)) * 1000 as total_time_ms,
    
    -- State flags
    ra.state IN ('RESERVED', 'ACTIVE', 'PARTIALLY_RELEASED') as is_active_route,
    ra.overlap_release_due_at IS NOT NULL AND ra.overlap_release_due_at <= CURRENT_TIMESTAMP as overlap_expired
    
FROM railway_control.route_assignments ra
LEFT JOIN railway_control.signals src_sig ON ra.source_signal_id = src_sig.signal_id
LEFT JOIN railway_control.signals dest_sig ON ra.dest_signal_id = dest_sig.signal_id;

-- Active routes summary
CREATE OR REPLACE VIEW railway_control.v_active_routes_summary AS
SELECT
    COUNT(*) as total_active_routes,
    COUNT(*) FILTER (WHERE state = 'RESERVED') as reserved_routes,
    COUNT(*) FILTER (WHERE state = 'ACTIVE') as active_routes,
    COUNT(*) FILTER (WHERE state = 'PARTIALLY_RELEASED') as partially_released_routes,
    COUNT(*) FILTER (WHERE overlap_release_due_at IS NOT NULL AND overlap_release_due_at <= CURRENT_TIMESTAMP) as expired_overlaps,
    AVG(EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - created_at)) * 1000) as avg_route_age_ms
FROM railway_control.route_assignments
WHERE state IN ('RESERVED', 'ACTIVE', 'PARTIALLY_RELEASED');

-- Resource utilization view
CREATE OR REPLACE VIEW railway_control.v_resource_utilization AS
SELECT
    'TRACK_CIRCUIT' as resource_type,
    COUNT(DISTINCT tc.circuit_id) as total_resources,
    COUNT(DISTINCT rl.resource_id) as locked_resources,
    ROUND((COUNT(DISTINCT rl.resource_id)::NUMERIC / COUNT(DISTINCT tc.circuit_id)) * 100, 2) as utilization_percentage
FROM railway_control.track_circuits tc
LEFT JOIN railway_control.resource_locks rl ON rl.resource_type = 'TRACK_CIRCUIT' AND rl.resource_id = tc.circuit_id AND rl.is_active = TRUE
WHERE tc.is_active = TRUE

UNION ALL

SELECT
    'POINT_MACHINE' as resource_type,
    COUNT(DISTINCT pm.machine_id) as total_resources,
    COUNT(DISTINCT rl.resource_id) as locked_resources,
    ROUND((COUNT(DISTINCT rl.resource_id)::NUMERIC / COUNT(DISTINCT pm.machine_id)) * 100, 2) as utilization_percentage
FROM railway_control.point_machines pm
LEFT JOIN railway_control.resource_locks rl ON rl.resource_type = 'POINT_MACHINE' AND rl.resource_id = pm.machine_id AND rl.is_active = TRUE;

COMMIT;

-- ============================================================================
-- SCHEMA EXTENSION COMPLETED
-- ============================================================================
-- This script extends the existing RailFlux database schema with comprehensive
-- route assignment capabilities while maintaining backward compatibility.
-- The new tables integrate seamlessly with existing InterlockingService and
-- DatabaseManager components.
-- ============================================================================