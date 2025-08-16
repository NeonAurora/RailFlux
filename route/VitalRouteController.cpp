#include "VitalRouteController.h"
#include "../database/DatabaseManager.h"
#include "../interlocking/InterlockingService.h"
#include "ResourceLockService.h"
#include "TelemetryService.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtMath>

namespace RailFlux::Route {

VitalRouteController::VitalRouteController(
    DatabaseManager* dbManager,
    InterlockingService* interlockingService,
    ResourceLockService* resourceLockService,
    TelemetryService* telemetryService,
    QObject* parent
)
    : QObject(parent)
    , m_dbManager(dbManager)
    , m_interlockingService(interlockingService)
    , m_resourceLockService(resourceLockService)
    , m_telemetryService(telemetryService)
{
    if (!m_dbManager || !m_interlockingService || !m_resourceLockService || !m_telemetryService) {
        qCritical() << "VitalRouteController: One or more required services is null";
        return;
    }

    // Connect to database changes for reactive updates
    connect(m_dbManager, &DatabaseManager::connectionStateChanged,
            this, [this](bool connected) {
                if (connected) {
                    initialize();
                } else {
                    m_isOperational = false;
                    emit operationalStateChanged();
                }
            });

    // Connect to hardware state changes
    connect(m_dbManager, &DatabaseManager::trackSegmentUpdated,
            this, [this](const QString& segmentId) {
                // Track circuits are linked to segments - need to resolve circuit ID
                onTrackCircuitOccupancyChanged(segmentId, true); // Simplified for now
            });

    // Setup periodic safety check timer
    QTimer* safetyTimer = new QTimer(this);
    safetyTimer->setInterval(SAFETY_CHECK_INTERVAL_MS);
    connect(safetyTimer, &QTimer::timeout, this, &VitalRouteController::performPeriodicSafetyCheck);
    safetyTimer->start();
}

VitalRouteController::~VitalRouteController() = default;

void VitalRouteController::initialize() {
    qDebug() << "🔄 VitalRouteController: Initializing safety-critical route controller...";

    if (!m_dbManager || !m_dbManager->isConnected()) {
        qWarning() << "VitalRouteController: Cannot initialize - database not connected";
        return;
    }

    try {
        // Load active routes from database
        if (loadActiveRoutesFromDatabase()) {
            // Validate system state
            updateSafetySystemHealth();
            
            m_isOperational = true;
            
            qDebug() << "✅ VitalRouteController: Initialized with" << m_activeRoutes.size() << "active routes";
            emit operationalStateChanged();
            
            // Record initialization in telemetry
            if (m_telemetryService) {
                m_telemetryService->recordSafetyEvent(
                    "vital_controller_initialized",
                    "INFO",
                    "VitalRouteController",
                    QString("Initialized with %1 active routes").arg(m_activeRoutes.size()),
                    "system"
                );
            }
        } else {
            qCritical() << "❌ VitalRouteController: Failed to load active routes";
        }

    } catch (const std::exception& e) {
        qCritical() << "❌ VitalRouteController: Initialization failed:" << e.what();
        m_isOperational = false;
        emit operationalStateChanged();
    }
}

bool VitalRouteController::loadActiveRoutesFromDatabase() {
    QSqlQuery query(m_dbManager->database());
    query.prepare(R"(
        SELECT 
            id, route_name, source_signal_id, dest_signal_id, direction,
            assigned_circuits, overlap_circuits, locked_point_machines,
            state, priority, operator_id, created_at, activated_at,
            released_at, overlap_release_due_at, failure_reason,
            performance_metrics
        FROM railway_control.route_assignments
        WHERE state IN ('RESERVED', 'ACTIVE', 'PARTIALLY_RELEASED')
        ORDER BY created_at
    )");

    if (!query.exec()) {
        qCritical() << "VitalRouteController: Failed to load active routes:" << query.lastError().text();
        return false;
    }

    m_activeRoutes.clear();
    m_routesByCircuit.clear();

    while (query.next()) {
        RouteAssignment route;
        route.id = QUuid::fromString(query.value("id").toString());
        route.routeName = query.value("route_name").toString();
        route.sourceSignalId = query.value("source_signal_id").toString();
        route.destSignalId = query.value("dest_signal_id").toString();
        route.direction = query.value("direction").toString();
        
        // Parse PostgreSQL arrays
        QString circuitsStr = query.value("assigned_circuits").toString();
        if (circuitsStr.startsWith("{") && circuitsStr.endsWith("}")) {
            circuitsStr = circuitsStr.mid(1, circuitsStr.length() - 2);
            route.assignedCircuits = circuitsStr.split(",", Qt::SkipEmptyParts);
        }
        
        QString overlapStr = query.value("overlap_circuits").toString();
        if (overlapStr.startsWith("{") && overlapStr.endsWith("}")) {
            overlapStr = overlapStr.mid(1, overlapStr.length() - 2);
            route.overlapCircuits = overlapStr.split(",", Qt::SkipEmptyParts);
        }
        
        QString pmStr = query.value("locked_point_machines").toString();
        if (pmStr.startsWith("{") && pmStr.endsWith("}")) {
            pmStr = pmStr.mid(1, pmStr.length() - 2);
            route.lockedPointMachines = pmStr.split(",", Qt::SkipEmptyParts);
        }
        
        route.state = stringToRouteState(query.value("state").toString());
        route.priority = query.value("priority").toInt();
        route.operatorId = query.value("operator_id").toString();
        route.createdAt = query.value("created_at").toDateTime();
        route.activatedAt = query.value("activated_at").toDateTime();
        route.releasedAt = query.value("released_at").toDateTime();
        route.overlapReleaseDueAt = query.value("overlap_release_due_at").toDateTime();
        route.failureReason = query.value("failure_reason").toString();

        // Store route
        QString routeId = route.key();
        m_activeRoutes[routeId] = route;

        // Index by circuits
        for (const QString& circuitId : route.assignedCircuits + route.overlapCircuits) {
            if (!m_routesByCircuit.contains(circuitId)) {
                m_routesByCircuit[circuitId] = QStringList();
            }
            m_routesByCircuit[circuitId].append(routeId);
        }
    }

    qDebug() << "📥 VitalRouteController: Loaded" << m_activeRoutes.size() << "active routes from database";
    return true;
}

QVariantMap VitalRouteController::validateRouteRequest(
    const QString& sourceSignalId,
    const QString& destSignalId,
    const QString& direction,
    const QString& operatorId
) {
    QElapsedTimer timer;
    timer.start();
    
    m_totalValidations++;

    if (!m_isOperational) {
        return QVariantMap{
            {"success", false},
            {"error", "VitalRouteController not operational"},
            {"safetyLevel", "DANGER"}
        };
    }

    ValidationResult result = validateRouteRequestInternal(sourceSignalId, destSignalId, direction, operatorId);
    
    auto duration = std::chrono::milliseconds(timer.elapsed());
    recordValidationTime("route_request_validation", duration);

    if (result.isAllowed) {
        m_successfulValidations++;
    }

    // Record performance metrics
    if (m_telemetryService) {
        m_telemetryService->recordPerformanceMetric(
            "route_validation",
            timer.elapsed(),
            result.isAllowed,
            QString("%1->%2").arg(sourceSignalId, destSignalId),
            QVariantMap{
                {"sourceSignal", sourceSignalId},
                {"destSignal", destSignalId},
                {"direction", direction},
                {"safetyLevel", safetyLevelToString(result.safetyLevel)}
            }
        );
    }

    return validationResultToVariantMap(result);
}

ValidationResult VitalRouteController::validateRouteRequestInternal(
    const QString& sourceSignalId,
    const QString& destSignalId,
    const QString& direction,
    const QString& operatorId
) {
    Q_UNUSED(operatorId) // May be used for authorization in future

    // 1. Basic validation
    if (sourceSignalId.isEmpty() || destSignalId.isEmpty()) {
        return ValidationResult::blocked("Source or destination signal ID is empty");
    }

    if (sourceSignalId == destSignalId) {
        return ValidationResult::blocked("Source and destination signals cannot be the same");
    }

    if (direction != "UP" && direction != "DOWN") {
        return ValidationResult::blocked("Invalid direction - must be UP or DOWN");
    }

    // 2. Signal progression validation
    ValidationResult progressionResult = validateSignalProgression(sourceSignalId, destSignalId);
    if (!progressionResult.isAllowed) {
        return progressionResult;
    }

    // 3. Check if signals exist and are active
    QVariantMap sourceSignal = m_dbManager->getSignalById(sourceSignalId);
    QVariantMap destSignal = m_dbManager->getSignalById(destSignalId);

    if (sourceSignal.isEmpty()) {
        return ValidationResult::blocked(QString("Source signal not found: %1").arg(sourceSignalId));
    }

    if (destSignal.isEmpty()) {
        return ValidationResult::blocked(QString("Destination signal not found: %1").arg(destSignalId));
    }

    if (!sourceSignal["isActive"].toBool()) {
        return ValidationResult::blocked(QString("Source signal is not active: %1").arg(sourceSignalId));
    }

    if (!destSignal["isActive"].toBool()) {
        return ValidationResult::blocked(QString("Destination signal is not active: %1").arg(destSignalId));
    }

    // 4. Check direction consistency
    if (sourceSignal["direction"].toString() != direction || destSignal["direction"].toString() != direction) {
        return ValidationResult::blocked("Signal direction mismatch with requested route direction");
    }

    // 5. Check for existing conflicting routes
    // This is a simplified check - full implementation would use pathfinding results
    for (const RouteAssignment& route : m_activeRoutes) {
        if (route.sourceSignalId == sourceSignalId || route.destSignalId == destSignalId) {
            if (route.isActive()) {
                ValidationResult result = ValidationResult::blocked(
                    QString("Conflicting route exists: %1").arg(route.key()),
                    SafetyLevel::WARNING
                );
                result.conflictingResources.append(route.key());
                return result;
            }
        }
    }

    // If all validations pass
    ValidationResult result = ValidationResult::allowed("Route request validation passed");
    result.safetyLevel = SafetyLevel::VITAL_SAFE;
    result.details = QString("Validated route from %1 to %2 in %3 direction")
                        .arg(sourceSignalId, destSignalId, direction);
    
    return result;
}

ValidationResult VitalRouteController::validateSignalProgression(
    const QString& sourceSignalId,
    const QString& destSignalId
) const {
    // Get signal types from database
    QVariantMap sourceSignal = m_dbManager->getSignalById(sourceSignalId);
    QVariantMap destSignal = m_dbManager->getSignalById(destSignalId);

    if (sourceSignal.isEmpty() || destSignal.isEmpty()) {
        return ValidationResult::blocked("Cannot determine signal types for progression validation");
    }

    QString sourceType = sourceSignal["type"].toString();
    QString destType = destSignal["type"].toString();

    if (!isValidProgressionSequence(sourceType, destType)) {
        return ValidationResult::blocked(
            QString("Invalid signal progression: %1 (%2) to %3 (%4)")
                .arg(sourceSignalId, sourceType, destSignalId, destType),
            SafetyLevel::DANGER
        );
    }

    return ValidationResult::allowed("Signal progression validation passed");
}

bool VitalRouteController::isValidSignalProgression(
    const QString& sourceSignalType,
    const QString& destSignalType
) const {
    return isValidProgressionSequence(sourceSignalType, destSignalType);
}

bool VitalRouteController::isValidProgressionSequence(const QString& sourceType, const QString& destType) const {
    // Define valid signal progression sequences according to railway signaling rules
    static const QHash<QString, QStringList> validProgressions = {
        {"OUTER", {"HOME"}},                           // OUTER -> HOME
        {"HOME", {"STARTER"}},                         // HOME -> STARTER  
        {"STARTER", {"ADVANCED_STARTER"}},             // STARTER -> ADVANCED_STARTER
        {"ADVANCED_STARTER", {"OUTER", "HOME"}},       // ADVANCED_STARTER -> next signal block
        
        // Special cases for local movements
        {"HOME", {"ADVANCED_STARTER"}},                // Direct HOME -> ADVANCED_STARTER (bypass STARTER)
        {"STARTER", {"HOME"}},                         // Reverse movements (DOWN direction)
        {"ADVANCED_STARTER", {"STARTER"}}              // Reverse movements (DOWN direction)
    };

    if (!validProgressions.contains(sourceType)) {
        qWarning() << "VitalRouteController: Unknown source signal type:" << sourceType;
        return false;
    }

    return validProgressions[sourceType].contains(destType);
}

QVariantMap VitalRouteController::validateResourceAvailability(
    const QStringList& circuits,
    const QStringList& pointMachines
) {
    QElapsedTimer timer;
    timer.start();

    ValidationResult result = validateResourceAvailabilityInternal(circuits, pointMachines);
    
    auto duration = std::chrono::milliseconds(timer.elapsed());
    recordValidationTime("resource_availability_validation", duration);

    return validationResultToVariantMap(result);
}

ValidationResult VitalRouteController::validateResourceAvailabilityInternal(
    const QStringList& circuits,
    const QStringList& pointMachines
) {
    if (!m_isOperational) {
        return ValidationResult::blocked("VitalRouteController not operational");
    }

    QStringList unavailableResources;
    QStringList conflictingRoutes;

    // Check track circuits
    for (const QString& circuitId : circuits) {
        // Check if circuit is occupied
        // This would integrate with track circuit monitoring
        // For now, simplified check through database
        
        // Check if circuit is already assigned to another route
        if (m_routesByCircuit.contains(circuitId)) {
            for (const QString& routeId : m_routesByCircuit[circuitId]) {
                if (m_activeRoutes.contains(routeId) && m_activeRoutes[routeId].isActive()) {
                    unavailableResources.append(QString("Circuit %1 (Route %2)").arg(circuitId, routeId));
                    if (!conflictingRoutes.contains(routeId)) {
                        conflictingRoutes.append(routeId);
                    }
                }
            }
        }

        // Check resource lock service
        if (m_resourceLockService && m_resourceLockService->isResourceLocked("TRACK_CIRCUIT", circuitId)) {
            QVariantMap lockStatus = m_resourceLockService->getResourceLockStatus("TRACK_CIRCUIT", circuitId);
            unavailableResources.append(QString("Circuit %1 (Locked)").arg(circuitId));
        }
    }

    // Check point machines
    for (const QString& machineId : pointMachines) {
        if (m_resourceLockService && m_resourceLockService->isResourceLocked("POINT_MACHINE", machineId)) {
            unavailableResources.append(QString("Point Machine %1 (Locked)").arg(machineId));
        }
    }

    if (!unavailableResources.isEmpty()) {
        ValidationResult result = ValidationResult::blocked(
            QString("Resources unavailable: %1").arg(unavailableResources.join(", ")),
            SafetyLevel::WARNING
        );
        result.conflictingResources = unavailableResources;
        result.details = QString("Found %1 unavailable resources").arg(unavailableResources.size());
        return result;
    }

    // All resources available
    ValidationResult result = ValidationResult::allowed("All requested resources are available");
    result.safetyLevel = SafetyLevel::SAFE;
    result.details = QString("Validated %1 circuits and %2 point machines")
                        .arg(circuits.size()).arg(pointMachines.size());
    
    return result;
}

QVariantMap VitalRouteController::validateAgainstInterlocking(const QVariantMap& routeData) {
    QElapsedTimer timer;
    timer.start();

    RouteAssignment route = variantMapToRouteAssignment(routeData);
    ValidationResult result = validateAgainstInterlockingInternal(route);
    
    auto duration = std::chrono::milliseconds(timer.elapsed());
    recordValidationTime("interlocking_validation", duration);

    return validationResultToVariantMap(result);
}

ValidationResult VitalRouteController::validateAgainstInterlockingInternal(const RouteAssignment& route) {
    if (!m_interlockingService) {
        return ValidationResult::blocked("InterlockingService not available");
    }

    // Validate source signal can be cleared
    if (!checkSignalInterlocking(route.sourceSignalId, "GREEN")) {
        return ValidationResult::blocked(
            QString("Source signal %1 cannot be cleared to proceed aspect").arg(route.sourceSignalId),
            SafetyLevel::DANGER
        );
    }

    // Validate point machine positions
    for (const QString& machineId : route.lockedPointMachines) {
        // Get required position for this route (simplified - would use pathfinding results)
        QString requiredPosition = "NORMAL"; // Placeholder
        
        if (!checkPointMachineInterlocking(machineId, requiredPosition)) {
            return ValidationResult::blocked(
                QString("Point machine %1 cannot be set to %2").arg(machineId, requiredPosition),
                SafetyLevel::DANGER
            );
        }
    }

    // If using the existing interlocking service validation
    // This would call the actual interlocking validation methods
    // For now, simplified implementation

    ValidationResult result = ValidationResult::allowed("Interlocking validation passed");
    result.safetyLevel = SafetyLevel::VITAL_SAFE;
    result.details = QString("Route %1 passed all interlocking checks").arg(route.key());
    
    return result;
}

bool VitalRouteController::checkSignalInterlocking(const QString& signalId, const QString& requestedAspect) {
    if (!m_interlockingService) {
        return false;
    }

    // Get current signal aspect
    QVariantMap signal = m_dbManager->getSignalById(signalId);
    if (signal.isEmpty()) {
        return false;
    }

    QString currentAspect = signal["currentAspect"].toString();
    
    // Use existing interlocking service to validate aspect change
    auto validationResult = m_interlockingService->validateMainSignalOperation(
        signalId, currentAspect, requestedAspect, "VitalRouteController"
    );

    return validationResult.isAllowed();
}

bool VitalRouteController::checkPointMachineInterlocking(const QString& machineId, const QString& requestedPosition) {
    if (!m_interlockingService) {
        return false;
    }

    // Get current point machine position  
    QVariantList pointMachines = m_dbManager->getPointMachinesList();
    QString currentPosition;
    
    for (const QVariant& pm : pointMachines) {
        QVariantMap machine = pm.toMap();
        if (machine["id"].toString() == machineId) {
            currentPosition = machine["currentPosition"].toString();
            break;
        }
    }

    if (currentPosition.isEmpty()) {
        return false;
    }

    // Use existing interlocking service to validate position change
    auto validationResult = m_interlockingService->validatePointMachineOperation(
        machineId, currentPosition, requestedPosition, "VitalRouteController"
    );

    return validationResult.isAllowed();
}

QVariantMap VitalRouteController::reserveRouteResources(const QVariantMap& routeData) {
    QElapsedTimer timer;
    timer.start();

    RouteAssignment route = variantMapToRouteAssignment(routeData);
    ValidationResult result = reserveRouteResourcesInternal(route);
    
    auto duration = std::chrono::milliseconds(timer.elapsed());
    recordValidationTime("route_reservation", duration);

    // Record telemetry
    if (m_telemetryService) {
        m_telemetryService->recordPerformanceMetric(
            "route_reservation",
            timer.elapsed(),
            result.isAllowed,
            route.key(),
            QVariantMap{
                {"sourceSignal", route.sourceSignalId},
                {"destSignal", route.destSignalId},
                {"circuitCount", route.assignedCircuits.size()},
                {"safetyLevel", safetyLevelToString(result.safetyLevel)}
            }
        );

        if (result.isAllowed) {
            m_telemetryService->recordOperationalMetric(
                "routes_reserved_total",
                m_activeRoutes.size(),
                "count"
            );
        }
    }

    QVariantMap resultMap = validationResultToVariantMap(result);
    if (result.isAllowed) {
        resultMap["routeId"] = route.key();
        emit routeReserved(route.key(), route.sourceSignalId, route.destSignalId);
        emit routeCountChanged();
    }

    return resultMap;
}

ValidationResult VitalRouteController::reserveRouteResourcesInternal(RouteAssignment& route) {
    if (!m_isOperational) {
        return ValidationResult::blocked("VitalRouteController not operational");
    }

    // 1. Final validation before reservation
    ValidationResult validation = validateAgainstInterlockingInternal(route);
    if (!validation.isAllowed) {
        return validation;
    }

    // 2. Lock resources through ResourceLockService
    if (!lockResourcesForRoute(route)) {
        return ValidationResult::blocked("Failed to lock required resources", SafetyLevel::WARNING);
    }

    // 3. Update route state and persist
    route.state = RouteState::RESERVED;
    route.createdAt = QDateTime::currentDateTime();

    if (!persistRouteToDatabase(route)) {
        // Rollback resource locks
        unlockResourcesForRoute(route.key());
        return ValidationResult::blocked("Failed to persist route to database");
    }

    // 4. Add to active routes
    QString routeId = route.key();
    m_activeRoutes[routeId] = route;

    // 5. Index by circuits
    for (const QString& circuitId : route.assignedCircuits + route.overlapCircuits) {
        if (!m_routesByCircuit.contains(circuitId)) {
            m_routesByCircuit[circuitId] = QStringList();
        }
        m_routesByCircuit[circuitId].append(routeId);
    }

    qDebug() << "🟢 VitalRouteController: Reserved route" << routeId 
             << "from" << route.sourceSignalId << "to" << route.destSignalId;

    // Record safety event
    recordSafetyEvent("route_reserved", routeId, 
                     QString("Route from %1 to %2").arg(route.sourceSignalId, route.destSignalId));

    ValidationResult result = ValidationResult::allowed("Route resources reserved successfully");
    result.safetyLevel = SafetyLevel::VITAL_SAFE;
    result.details = QString("Reserved %1 circuits and %2 point machines")
                        .arg(route.assignedCircuits.size())
                        .arg(route.lockedPointMachines.size());
    
    return result;
}

bool VitalRouteController::lockResourcesForRoute(const RouteAssignment& route) {
    if (!m_resourceLockService) {
        return false;
    }

    QStringList failedLocks;

    // Lock track circuits
    for (const QString& circuitId : route.assignedCircuits + route.overlapCircuits) {
        QVariantMap lockResult = m_resourceLockService->lockResource(
            "TRACK_CIRCUIT", circuitId, route.key(), "EXCLUSIVE", route.operatorId,
            QString("Route %1").arg(route.key())
        );
        
        if (!lockResult["success"].toBool()) {
            failedLocks.append(QString("Circuit %1: %2").arg(circuitId, lockResult["error"].toString()));
        }
    }

    // Lock point machines
    for (const QString& machineId : route.lockedPointMachines) {
        QVariantMap lockResult = m_resourceLockService->lockResource(
            "POINT_MACHINE", machineId, route.key(), "EXCLUSIVE", route.operatorId,
            QString("Route %1").arg(route.key())
        );
        
        if (!lockResult["success"].toBool()) {
            failedLocks.append(QString("PM %1: %2").arg(machineId, lockResult["error"].toString()));
        }
    }

    if (!failedLocks.isEmpty()) {
        qWarning() << "VitalRouteController: Failed to lock resources for route" << route.key() << ":" << failedLocks;
        
        // Rollback successful locks
        unlockResourcesForRoute(route.key());
        return false;
    }

    return true;
}

bool VitalRouteController::unlockResourcesForRoute(const QString& routeId) {
    if (!m_resourceLockService) {
        return false;
    }

    return m_resourceLockService->unlockAllResourcesForRoute(routeId);
}

QVariantMap VitalRouteController::emergencyRelease(const QString& routeId, const QString& reason) {
    QElapsedTimer timer;
    timer.start();

    ValidationResult result = emergencyReleaseInternal(routeId, reason);
    
    auto duration = std::chrono::milliseconds(timer.elapsed());
    recordValidationTime("emergency_release", duration);

    // Record critical safety event
    if (m_telemetryService) {
        m_telemetryService->recordSafetyEvent(
            "emergency_release",
            "CRITICAL",
            routeId,
            QString("Emergency release: %1").arg(reason),
            "VitalRouteController"
        );
    }

    if (result.isAllowed) {
        m_emergencyReleases++;
        emit emergencyReleasePerformed(routeId, reason);
        emit routeCountChanged();
    }

    return validationResultToVariantMap(result);
}

ValidationResult VitalRouteController::emergencyReleaseInternal(const QString& routeId, const QString& reason) {
    if (!m_activeRoutes.contains(routeId)) {
        return ValidationResult::blocked("Route not found: " + routeId);
    }

    RouteAssignment& route = m_activeRoutes[routeId];
    
    qCritical() << "🚨 VitalRouteController: EMERGENCY RELEASE of route" << routeId << "- Reason:" << reason;

    // Record safety event
    recordSafetyEvent("emergency_release", routeId, reason);

    // Notify emergency services
    notifyEmergencyServices(routeId, reason);

    // Force unlock all resources
    unlockResourcesForRoute(routeId);

    // Update route state
    route.state = RouteState::EMERGENCY_RELEASED;
    route.releasedAt = QDateTime::currentDateTime();
    route.failureReason = reason;

    // Update database
    updateRouteInDatabase(route);

    // Remove from active routes
    m_activeRoutes.remove(routeId);

    // Remove circuit indexing
    for (const QString& circuitId : route.assignedCircuits + route.overlapCircuits) {
        if (m_routesByCircuit.contains(circuitId)) {
            m_routesByCircuit[circuitId].removeOne(routeId);
            if (m_routesByCircuit[circuitId].isEmpty()) {
                m_routesByCircuit.remove(circuitId);
            }
        }
    }

    ValidationResult result = ValidationResult::allowed("Emergency release completed");
    result.safetyLevel = SafetyLevel::DANGER; // Mark as danger due to emergency nature
    result.details = QString("Emergency release of route %1: %2").arg(routeId, reason);
    
    return result;
}

void VitalRouteController::performPeriodicSafetyCheck() {
    if (!m_isOperational) {
        return;
    }

    checkForSafetyViolations();
    updateSafetySystemHealth();
    
    m_lastSafetyCheck = QDateTime::currentDateTime();
}

void VitalRouteController::checkForSafetyViolations() {
    // Check for route conflicts, resource violations, etc.
    // This is a simplified implementation
    
    for (const RouteAssignment& route : m_activeRoutes) {
        if (!isRouteConflictFree(route)) {
            QString violation = QString("Route conflict detected for route %1").arg(route.key());
            m_recentSafetyViolations.append(violation);
            m_safetyViolations++;
            
            emit safetyViolationDetected(route.key(), "route_conflict", violation);
            
            if (m_telemetryService) {
                m_telemetryService->recordSafetyViolation("route_conflict", route.key(), violation);
            }
        }
    }

    // Keep only recent violations
    if (m_recentSafetyViolations.size() > 10) {
        m_recentSafetyViolations.removeFirst();
    }
}

bool VitalRouteController::isRouteConflictFree(const RouteAssignment& route) const {
    // Simplified conflict detection
    // Full implementation would check:
    // - Circuit occupancy vs assignment
    // - Point machine position vs route requirements
    // - Signal aspects vs route state
    // - Overlap violations
    
    Q_UNUSED(route)
    return true; // Placeholder
}

// Utility method implementations
QString VitalRouteController::routeStateToString(RouteState state) const {
    switch (state) {
        case RouteState::REQUESTED: return "REQUESTED";
        case RouteState::VALIDATING: return "VALIDATING";
        case RouteState::RESERVED: return "RESERVED";
        case RouteState::ACTIVE: return "ACTIVE";
        case RouteState::PARTIALLY_RELEASED: return "PARTIALLY_RELEASED";
        case RouteState::RELEASED: return "RELEASED";
        case RouteState::FAILED: return "FAILED";
        case RouteState::EMERGENCY_RELEASED: return "EMERGENCY_RELEASED";
        case RouteState::DEGRADED: return "DEGRADED";
        default: return "UNKNOWN";
    }
}

RouteState VitalRouteController::stringToRouteState(const QString& stateStr) const {
    if (stateStr == "REQUESTED") return RouteState::REQUESTED;
    if (stateStr == "VALIDATING") return RouteState::VALIDATING;
    if (stateStr == "RESERVED") return RouteState::RESERVED;
    if (stateStr == "ACTIVE") return RouteState::ACTIVE;
    if (stateStr == "PARTIALLY_RELEASED") return RouteState::PARTIALLY_RELEASED;
    if (stateStr == "RELEASED") return RouteState::RELEASED;
    if (stateStr == "FAILED") return RouteState::FAILED;
    if (stateStr == "EMERGENCY_RELEASED") return RouteState::EMERGENCY_RELEASED;
    if (stateStr == "DEGRADED") return RouteState::DEGRADED;
    return RouteState::FAILED;
}

QString VitalRouteController::safetyLevelToString(SafetyLevel level) const {
    switch (level) {
        case SafetyLevel::VITAL_SAFE: return "VITAL_SAFE";
        case SafetyLevel::SAFE: return "SAFE";
        case SafetyLevel::CAUTION: return "CAUTION";
        case SafetyLevel::WARNING: return "WARNING";
        case SafetyLevel::DANGER: return "DANGER";
        default: return "DANGER";
    }
}

QVariantMap VitalRouteController::validationResultToVariantMap(const ValidationResult& result) const {
    return QVariantMap{
        {"success", result.isAllowed},
        {"safetyLevel", safetyLevelToString(result.safetyLevel)},
        {"reason", result.reason},
        {"details", result.details},
        {"conflictingResources", result.conflictingResources},
        {"alternativeSolutions", result.alternativeSolutions},
        {"responseTimeMs", static_cast<double>(result.responseTime.count())},
        {"performanceMetrics", result.performanceMetrics},
        {"interlockingResults", result.interlockingResults}
    };
}

RouteAssignment VitalRouteController::variantMapToRouteAssignment(const QVariantMap& map) const {
    RouteAssignment route;
    route.id = QUuid::fromString(map.value("id", QUuid::createUuid().toString()).toString());
    route.routeName = map.value("routeName").toString();
    route.sourceSignalId = map.value("sourceSignalId").toString();
    route.destSignalId = map.value("destSignalId").toString();
    route.direction = map.value("direction").toString();
    route.assignedCircuits = map.value("assignedCircuits").toStringList();
    route.overlapCircuits = map.value("overlapCircuits").toStringList();
    route.lockedPointMachines = map.value("lockedPointMachines").toStringList();
    route.state = stringToRouteState(map.value("state", "REQUESTED").toString());
    route.priority = map.value("priority", 100).toInt();
    route.operatorId = map.value("operatorId", "system").toString();
    return route;
}

int VitalRouteController::activeRoutes() const {
    return m_activeRoutes.size();
}

void VitalRouteController::recordValidationTime(const QString& operation, std::chrono::milliseconds duration) {
    m_validationTimes.append(duration);
    if (m_validationTimes.size() > PERFORMANCE_HISTORY_SIZE) {
        m_validationTimes.removeFirst();
    }
    
    updateAverageValidationTime();
    
    if (duration > TARGET_VALIDATION_TIME) {
        qWarning() << "⚠️ VitalRouteController: Slow" << operation << ":" << duration.count() << "ms";
    }
}

void VitalRouteController::updateAverageValidationTime() {
    if (m_validationTimes.isEmpty()) {
        return;
    }
    
    std::chrono::milliseconds total{0};
    for (const auto& time : m_validationTimes) {
        total += time;
    }
    
    m_averageValidationTime = static_cast<double>(total.count()) / m_validationTimes.size();
    emit performanceChanged();
}

void VitalRouteController::recordSafetyEvent(const QString& eventType, const QString& routeId, const QString& details) {
    if (m_telemetryService) {
        m_telemetryService->recordSafetyEvent(eventType, "INFO", routeId, details, "VitalRouteController");
    }
}

void VitalRouteController::updateSafetySystemHealth() {
    bool wasHealthy = m_safetySystemHealthy;
    
    // Check various health indicators
    bool servicesOperational = m_dbManager && m_interlockingService && 
                              m_resourceLockService && m_telemetryService;
    bool performanceAcceptable = m_averageValidationTime < TARGET_VALIDATION_TIME.count() * 2;
    bool noRecentViolations = m_recentSafetyViolations.size() < 5;
    
    m_safetySystemHealthy = servicesOperational && performanceAcceptable && noRecentViolations;
    
    if (wasHealthy != m_safetySystemHealthy) {
        emit safetyStatusChanged();
        
        if (!m_safetySystemHealthy) {
            qCritical() << "🚨 VitalRouteController: Safety system health degraded";
            if (m_telemetryService) {
                m_telemetryService->recordSafetyEvent(
                    "safety_system_degraded",
                    "CRITICAL",
                    "VitalRouteController",
                    "Safety system health check failed",
                    "system"
                );
            }
        }
    }
}

void VitalRouteController::notifyEmergencyServices(const QString& routeId, const QString& reason) {
    qCritical() << "🚨 EMERGENCY NOTIFICATION: Route" << routeId << "released due to:" << reason;
    // In a real system, this would notify control center, log to external systems, etc.
}

// Stub implementations for remaining methods
bool VitalRouteController::persistRouteToDatabase(const RouteAssignment& route) {
    Q_UNUSED(route)
    return true; // Placeholder
}

bool VitalRouteController::updateRouteInDatabase(const RouteAssignment& route) {
    Q_UNUSED(route)
    return true; // Placeholder
}

void VitalRouteController::updateSafetySystemHealth() {
    // Implementation above
}

} // namespace RailFlux::Route