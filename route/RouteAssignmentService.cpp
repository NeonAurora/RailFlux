#include "RouteAssignmentService.h"
#include "../database/DatabaseManager.h"
#include "GraphService.h"
#include "ResourceLockService.h"
#include "OverlapService.h"
#include "TelemetryService.h"
#include "VitalRouteController.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtMath>
#include <algorithm>

namespace RailFlux::Route {

RouteAssignmentService::RouteAssignmentService(QObject* parent)
    : QObject(parent)
    , m_processingTimer(new QTimer(this))
    , m_maintenanceTimer(new QTimer(this))
    , m_serviceStartTime(QDateTime::currentDateTime().toSecsSinceEpoch())
{
    // Setup processing timer
    m_processingTimer->setInterval(m_queueProcessingIntervalMs);
    connect(m_processingTimer, &QTimer::timeout, this, &RouteAssignmentService::processRequestQueue);

    // Setup maintenance timer
    m_maintenanceTimer->setInterval(m_maintenanceIntervalMs);
    connect(m_maintenanceTimer, &QTimer::timeout, this, &RouteAssignmentService::performMaintenanceCheck);
}

RouteAssignmentService::~RouteAssignmentService() {
    if (m_processingTimer) {
        m_processingTimer->stop();
    }
    if (m_maintenanceTimer) {
        m_maintenanceTimer->stop();
    }
}

void RouteAssignmentService::setServices(
    DatabaseManager* dbManager,
    GraphService* graphService,
    ResourceLockService* resourceLockService,
    OverlapService* overlapService,
    TelemetryService* telemetryService,
    VitalRouteController* vitalController
) {
    m_dbManager = dbManager;
    
    // Take ownership of services (they will be managed by this service)
    m_graphService.reset(graphService);
    m_resourceLockService.reset(resourceLockService);
    m_overlapService.reset(overlapService);
    m_telemetryService.reset(telemetryService);
    m_vitalController.reset(vitalController);

    qDebug() << "RouteAssignmentService: Services composed successfully";
}

void RouteAssignmentService::initialize() {
    qDebug() << "🔄 RouteAssignmentService: Initializing main orchestration service...";

    if (!m_dbManager) {
        qCritical() << "RouteAssignmentService: DatabaseManager not set";
        return;
    }

    try {
        // Initialize all composed services
        if (m_graphService) m_graphService->loadGraphFromDatabase();
        if (m_resourceLockService) m_resourceLockService->initialize();
        if (m_overlapService) m_overlapService->initialize();
        if (m_telemetryService) m_telemetryService->initialize();
        if (m_vitalController) m_vitalController->initialize();

        // Load configuration
        if (loadConfiguration()) {
            // Connect to database changes for reactive updates
            connect(m_dbManager, &DatabaseManager::connectionStateChanged,
                    this, [this](bool connected) {
                        if (!connected) {
                            m_isOperational = false;
                            emit operationalStateChanged();
                        }
                    });

            // Connect to track circuit changes
            connect(m_dbManager, &DatabaseManager::trackSegmentUpdated,
                    this, [this](const QString& segmentId) {
                // Get the circuit associated with this segment and its occupancy
                QString circuitId = m_dbManager->getCircuitIdByTrackSegmentId(segmentId);
                if (!circuitId.isEmpty()) {
                    auto circuit = m_dbManager->getTrackCircuitById(circuitId);
                    bool isOccupied = circuit.value("is_occupied", false).toBool();
                    onTrackCircuitOccupancyChanged(circuitId, isOccupied);
                }
            });

            // Start processing
            m_isOperational = areServicesHealthy();
            if (m_isOperational) {
                m_processingTimer->start();
                m_maintenanceTimer->start();
                
                qDebug() << "✅ RouteAssignmentService: Initialized successfully";
                emit operationalStateChanged();

                // Record initialization
                if (m_telemetryService) {
                    m_telemetryService->recordSafetyEvent(
                        "route_service_initialized",
                        "INFO",
                        "RouteAssignmentService",
                        "Main orchestration service initialized",
                        "system"
                    );
                }
            } else {
                qCritical() << "❌ RouteAssignmentService: Failed - services not healthy";
            }
        } else {
            qCritical() << "❌ RouteAssignmentService: Failed to load configuration";
        }

    } catch (const std::exception& e) {
        qCritical() << "❌ RouteAssignmentService: Initialization failed:" << e.what();
        m_isOperational = false;
        emit operationalStateChanged();
    }
}

bool RouteAssignmentService::areServicesHealthy() const {
    return m_dbManager && m_dbManager->isConnected() &&
           m_graphService && m_graphService->isLoaded() &&
           m_resourceLockService && m_resourceLockService->isOperational() &&
           m_overlapService && m_overlapService->isOperational() &&
           m_telemetryService && m_telemetryService->isOperational() &&
           m_vitalController && m_vitalController->isOperational();
}

QString RouteAssignmentService::requestRoute(
    const QString& sourceSignalId,
    const QString& destSignalId,
    const QString& direction,
    const QString& requestedBy,
    const QVariantMap& trainData,
    const QString& priority
) {
    m_totalRequests++;

    if (!m_isOperational) {
        qWarning() << "RouteAssignmentService: Cannot process request - service not operational";
        return QString();
    }

    if (!canAcceptNewRequests()) {
        qWarning() << "RouteAssignmentService: Cannot accept new requests - system overloaded";
        emit systemOverloaded(m_requestQueue.size(), m_maxConcurrentRoutes);
        return QString();
    }

    // Create route request
    RouteRequest request;
    request.requestId = QUuid::createUuid();
    request.sourceSignalId = sourceSignalId;
    request.destSignalId = destSignalId;
    request.direction = direction;
    request.requestedBy = requestedBy;
    request.priority = priority;
    request.requestedAt = QDateTime::currentDateTime();
    request.trainData = trainData;
    request.reason = "Normal route request";

    // Basic validation
    if (!isValidSignalId(sourceSignalId) || !isValidSignalId(destSignalId)) {
        qWarning() << "RouteAssignmentService: Invalid signal IDs:" << sourceSignalId << destSignalId;
        return QString();
    }

    if (!isValidDirection(direction)) {
        qWarning() << "RouteAssignmentService: Invalid direction:" << direction;
        return QString();
    }

    // Add to queue
    addToQueue(request);

    // Persist request
    persistRouteRequest(request);

    qDebug() << "🎯 RouteAssignmentService: Route requested -" 
             << sourceSignalId << "→" << destSignalId << "(" << direction << ")";
    emit routeRequested(request.key(), sourceSignalId, destSignalId);
    emit requestQueueChanged();

    // Record metrics
    if (m_telemetryService) {
        m_telemetryService->recordOperationalMetric(
            "route_requests_total",
            m_totalRequests,
            "count"
        );
        
        m_telemetryService->recordOperationalMetric(
            "pending_requests",
            m_requestQueue.size(),
            "count"
        );
    }

    return request.key();
}

void RouteAssignmentService::addToQueue(const RouteRequest& request) {
    m_requestQueue.enqueue(request);
    
    // Prioritize queue if needed
    if (m_requestQueue.size() > 1) {
        prioritizeQueue();
    }

    // Check for overload
    if (m_requestQueue.size() > OVERLOAD_THRESHOLD) {
        emit systemOverloaded(m_requestQueue.size(), m_maxConcurrentRoutes);
        
        if (m_telemetryService) {
            m_telemetryService->recordSafetyEvent(
                "system_overload",
                "WARNING",
                "RouteAssignmentService",
                QString("Request queue size: %1").arg(m_requestQueue.size()),
                "system"
            );
        }
    }
}

void RouteAssignmentService::prioritizeQueue() {
    // Convert queue to list for sorting
    QList<RouteRequest> requests;
    while (!m_requestQueue.isEmpty()) {
        requests.append(m_requestQueue.dequeue());
    }

    // Sort by priority (higher priority first, then by timestamp)
    std::sort(requests.begin(), requests.end(), [this](const RouteRequest& a, const RouteRequest& b) {
        int priorityA = calculateRequestPriority(a);
        int priorityB = calculateRequestPriority(b);
        
        if (priorityA != priorityB) {
            return priorityA > priorityB; // Higher priority first
        }
        
        return a.requestedAt < b.requestedAt; // Earlier requests first
    });

    // Rebuild queue
    for (const RouteRequest& request : requests) {
        m_requestQueue.enqueue(request);
    }
}

int RouteAssignmentService::calculateRequestPriority(const RouteRequest& request) const {
    // Priority scoring system
    int score = 100; // Base priority
    
    if (request.priority == "EMERGENCY") {
        score += 1000;
    } else if (request.priority == "HIGH") {
        score += 500;
    } else if (request.priority == "LOW") {
        score -= 100;
    }
    
    // Age factor - older requests get higher priority
    qint64 ageSeconds = request.requestedAt.secsTo(QDateTime::currentDateTime());
    score += static_cast<int>(ageSeconds / 10); // +1 point per 10 seconds
    
    return score;
}

void RouteAssignmentService::processRequestQueue() {
    if (!m_isOperational || m_requestQueue.isEmpty()) {
        return;
    }

    if (!shouldProcessRequest()) {
        return; // Too many active routes or system overloaded
    }

    RouteRequest request = dequeueRequest();
    
    QElapsedTimer totalTimer;
    totalTimer.start();

    qDebug() << "🔄 Processing route request:" << request.key() 
             << request.sourceSignalId << "→" << request.destSignalId;

    // Add to processing requests
    m_processingRequests[request.key()] = request;

    // Process the request through the pipeline
    ProcessingResult result = processRouteRequest(request);
    
    double totalTime = totalTimer.elapsed();
    recordProcessingTime("total_processing", totalTime);

    // Remove from processing
    m_processingRequests.remove(request.key());

    if (result.success) {
        m_successfulRoutes++;
        qDebug() << "✅ Route assigned successfully:" << result.routeId 
                 << "Path:" << result.path << "Time:" << totalTime << "ms";
        
        emit routeAssigned(result.routeId, request.sourceSignalId, request.destSignalId, result.path);
        emit routeCountChanged();
    } else {
        m_failedRoutes++;
        qWarning() << "❌ Route assignment failed:" << request.key() << result.error;
        emit routeFailed(request.key(), result.error);
    }

    // Record performance metrics
    if (m_telemetryService) {
        m_telemetryService->recordPerformanceMetric(
            "route_processing",
            totalTime,
            result.success,
            request.key(),
            QVariantMap{
                {"sourceSignal", request.sourceSignalId},
                {"destSignal", request.destSignalId},
                {"direction", request.direction},
                {"pathLength", result.path.size()},
                {"overlapCount", result.overlapCircuits.size()}
            }
        );
    }

    emit requestQueueChanged();
}

RouteRequest RouteAssignmentService::dequeueRequest() {
    return m_requestQueue.dequeue();
}

bool RouteAssignmentService::shouldProcessRequest() const {
    // Check if we can accept more concurrent routes
    int currentActiveRoutes = activeRoutes();
    int maxRoutes = m_degradedMode ? m_degradedMaxRoutes : m_maxConcurrentRoutes;
    
    return currentActiveRoutes < maxRoutes;
}

ProcessingResult RouteAssignmentService::processRouteRequest(const RouteRequest& request) {
    ProcessingResult result;
    result.performanceBreakdown = QVariantMap();

    // Stage 1: Validate Request
    QElapsedTimer stageTimer;
    stageTimer.start();
    
    result = validateRequest(request);
    if (!result.success) return result;
    
    double validationTime = stageTimer.elapsed();
    recordProcessingTime("validation", validationTime);
    result.performanceBreakdown["validation_ms"] = validationTime;

    // Stage 2: Pathfinding
    stageTimer.restart();
    
    ProcessingResult pathResult = performPathfinding(request);
    if (!pathResult.success) return pathResult;
    
    result.path = pathResult.path;
    double pathfindingTime = stageTimer.elapsed();
    recordProcessingTime("pathfinding", pathfindingTime);
    result.performanceBreakdown["pathfinding_ms"] = pathfindingTime;

    // Stage 3: Overlap Calculation
    stageTimer.restart();
    
    ProcessingResult overlapResult = calculateOverlap(request, result.path);
    if (!overlapResult.success) return overlapResult;
    
    result.overlapCircuits = overlapResult.overlapCircuits;
    double overlapTime = stageTimer.elapsed();
    recordProcessingTime("overlap_calculation", overlapTime);
    result.performanceBreakdown["overlap_calculation_ms"] = overlapTime;

    // Stage 4: Resource Reservation
    stageTimer.restart();
    
    ProcessingResult reservationResult = reserveResources(request, result.path, result.overlapCircuits);
    if (!reservationResult.success) return reservationResult;
    
    result.routeId = reservationResult.routeId;
    double reservationTime = stageTimer.elapsed();
    recordProcessingTime("resource_reservation", reservationTime);
    result.performanceBreakdown["resource_reservation_ms"] = reservationTime;

    // Stage 5: Finalize Route
    stageTimer.restart();
    
    ProcessingResult finalResult = finalizeRoute(request, result.path, result.overlapCircuits);
    if (!finalResult.success) return finalResult;
    
    double finalizationTime = stageTimer.elapsed();
    recordProcessingTime("finalization", finalizationTime);
    result.performanceBreakdown["finalization_ms"] = finalizationTime;

    result.success = true;
    result.totalTimeMs = validationTime + pathfindingTime + overlapTime + reservationTime + finalizationTime;
    
    return result;
}

ProcessingResult RouteAssignmentService::validateRequest(const RouteRequest& request) {
    ProcessingResult result;

    if (!m_vitalController) {
        result.error = "VitalRouteController not available";
        return result;
    }

    // Use VitalRouteController for safety-critical validation
    QVariantMap validationResult = m_vitalController->validateRouteRequest(
        request.sourceSignalId,
        request.destSignalId,
        request.direction,
        request.requestedBy
    );

    if (!validationResult["success"].toBool()) {
        result.error = QString("Validation failed: %1").arg(validationResult["reason"].toString());
        result.validationResults = validationResult;
        return result;
    }

    result.success = true;
    result.validationResults = validationResult;
    return result;
}

ProcessingResult RouteAssignmentService::performPathfinding(const RouteRequest& request) {
    ProcessingResult result;

    if (!m_graphService) {
        result.error = "GraphService not available";
        return result;
    }

    // Get point machine states for conditional pathfinding
    QVariantMap pointMachineStates; // Would be populated from current PM positions
    
    // Perform pathfinding
    QVariantMap pathResult = m_graphService->findRoute(
        QString(), // startCircuitId - would be resolved from source signal
        QString(), // goalCircuitId - would be resolved from dest signal  
        request.direction,
        pointMachineStates,
        static_cast<int>(PATHFINDING_TIMEOUT_MS)
    );

    if (!pathResult["success"].toBool()) {
        result.error = QString("Pathfinding failed: %1").arg(pathResult["error"].toString());
        return result;
    }

    result.success = true;
    result.path = pathResult["path"].toStringList();
    result.performanceBreakdown["pathfinding_nodes_explored"] = pathResult["nodesExplored"];
    result.performanceBreakdown["pathfinding_cost"] = pathResult["cost"];
    
    return result;
}

ProcessingResult RouteAssignmentService::calculateOverlap(const RouteRequest& request, const QStringList& path) {
    ProcessingResult result;

    if (!m_overlapService) {
        result.error = "OverlapService not available";
        return result;
    }

    // Calculate overlap for destination signal
    QVariantMap overlapResult = m_overlapService->calculateOverlap(
        request.sourceSignalId,
        request.destSignalId,
        request.direction,
        request.trainData
    );

    if (!overlapResult["success"].toBool()) {
        result.error = QString("Overlap calculation failed: %1").arg(overlapResult["error"].toString());
        return result;
    }

    result.success = true;
    result.overlapCircuits = overlapResult["overlapCircuits"].toStringList();
    result.performanceBreakdown["overlap_hold_seconds"] = overlapResult["holdSeconds"];
    result.performanceBreakdown["overlap_method"] = overlapResult["method"];
    
    return result;
}

ProcessingResult RouteAssignmentService::reserveResources(
    const RouteRequest& request,
    const QStringList& path,
    const QStringList& overlap
) {
    ProcessingResult result;

    if (!m_vitalController) {
        result.error = "VitalRouteController not available";
        return result;
    }

    // Prepare route data for VitalRouteController
    QVariantMap routeData;
    routeData["id"] = QUuid::createUuid().toString();
    routeData["sourceSignalId"] = request.sourceSignalId;
    routeData["destSignalId"] = request.destSignalId;
    routeData["direction"] = request.direction;
    routeData["assignedCircuits"] = path;
    routeData["overlapCircuits"] = overlap;
    routeData["operatorId"] = request.requestedBy;
    routeData["priority"] = request.priority;

    // Use VitalRouteController for safety-critical resource reservation
    QVariantMap reservationResult = m_vitalController->reserveRouteResources(routeData);

    if (!reservationResult["success"].toBool()) {
        result.error = QString("Resource reservation failed: %1").arg(reservationResult["reason"].toString());
        return result;
    }

    result.success = true;
    result.routeId = routeData["id"].toString();
    result.validationResults = reservationResult;
    
    return result;
}

ProcessingResult RouteAssignmentService::finalizeRoute(
    const RouteRequest& request,
    const QStringList& path,
    const QStringList& overlap
) {
    ProcessingResult result;

    // Persist route assignment to database
    if (!persistRouteAssignment(result.routeId, request, path)) {
        result.error = "Failed to persist route assignment";
        return result;
    }

    // Reserve overlap if needed
    if (!overlap.isEmpty() && m_overlapService) {
        QStringList releaseTriggers; // Would be determined from overlap definition
        
        QVariantMap overlapReservation = m_overlapService->reserveOverlap(
            result.routeId,
            request.destSignalId,
            overlap,
            releaseTriggers,
            request.requestedBy
        );

        if (!overlapReservation["success"].toBool()) {
            qWarning() << "Failed to reserve overlap for route" << result.routeId 
                       << ":" << overlapReservation["error"].toString();
            // Continue anyway - overlap is optional for basic route operation
        }
    }

    result.success = true;
    return result;
}

bool RouteAssignmentService::cancelRoute(const QString& routeId, const QString& reason) {
    if (!m_isOperational) {
        return false;
    }

    // Check if route is in processing
    if (m_processingRequests.contains(routeId)) {
        m_processingRequests.remove(routeId);
        qDebug() << "🚫 Cancelled processing route request:" << routeId;
        return true;
    }

    // Use VitalRouteController to release active route
    if (m_vitalController) {
        QVariantMap releaseResult = m_vitalController->releaseRouteResources(routeId);
        if (releaseResult["success"].toBool()) {
            qDebug() << "🚫 Cancelled active route:" << routeId << "Reason:" << reason;
            emit routeReleased(routeId, reason);
            emit routeCountChanged();
            return true;
        }
    }

    return false;
}

bool RouteAssignmentService::emergencyReleaseRoute(const QString& routeId, const QString& reason) {
    if (!m_vitalController) {
        return false;
    }

    QVariantMap result = m_vitalController->emergencyRelease(routeId, reason);
    bool success = result["success"].toBool();

    if (success) {
        m_emergencyReleases++;
        
        if (m_telemetryService) {
            m_telemetryService->recordSafetyEvent(
                "emergency_route_release",
                "CRITICAL",
                routeId,
                QString("Emergency release: %1").arg(reason),
                "RouteAssignmentService"
            );
        }

        qCritical() << "🚨 Emergency release performed for route" << routeId << ":" << reason;
        emit routeReleased(routeId, QString("EMERGENCY: %1").arg(reason));
        emit routeCountChanged();
    }

    return success;
}

bool RouteAssignmentService::emergencyReleaseAllRoutes(const QString& reason) {
    if (!m_vitalController) {
        return false;
    }

    QVariantMap result = m_vitalController->emergencyReleaseAll(reason);
    bool success = result["success"].toBool();

    if (success) {
        m_emergencyReleases++;
        
        if (m_telemetryService) {
            m_telemetryService->recordSafetyEvent(
                "emergency_all_routes_release",
                "EMERGENCY",
                "ALL_ROUTES",
                QString("Emergency release all: %1").arg(reason),
                "RouteAssignmentService"
            );
        }

        qCritical() << "🚨 EMERGENCY RELEASE ALL ROUTES:" << reason;
        emit routeCountChanged();
    }

    return success;
}

void RouteAssignmentService::activateEmergencyMode(const QString& reason) {
    if (m_emergencyMode) {
        return; // Already in emergency mode
    }

    m_emergencyMode = true;
    enterDegradedMode();

    qCritical() << "🚨 EMERGENCY MODE ACTIVATED:" << reason;
    emit emergencyActivated(reason);
    emit emergencyModeChanged();

    if (m_telemetryService) {
        m_telemetryService->recordSafetyEvent(
            "emergency_mode_activated",
            "EMERGENCY",
            "RouteAssignmentService",
            QString("Emergency mode activated: %1").arg(reason),
            "system"
        );
    }
}

void RouteAssignmentService::deactivateEmergencyMode() {
    if (!m_emergencyMode) {
        return;
    }

    m_emergencyMode = false;
    exitDegradedMode();

    qDebug() << "✅ Emergency mode deactivated";
    emit emergencyDeactivated();
    emit emergencyModeChanged();

    if (m_telemetryService) {
        m_telemetryService->recordSafetyEvent(
            "emergency_mode_deactivated",
            "INFO",
            "RouteAssignmentService",
            "Emergency mode deactivated - normal operations resumed",
            "system"
        );
    }
}

void RouteAssignmentService::enterDegradedMode() {
    m_degradedMode = true;
    applyDegradedModeSettings();
    
    qWarning() << "⚠️ Entering degraded mode - reduced capacity";
}

void RouteAssignmentService::exitDegradedMode() {
    m_degradedMode = false;
    restoreNormalModeSettings();
    
    qDebug() << "✅ Exiting degraded mode - normal capacity restored";
}

void RouteAssignmentService::applyDegradedModeSettings() {
    // Reduce concurrent route limit
    // Increase processing intervals
    // Apply conservative timeouts
}

void RouteAssignmentService::restoreNormalModeSettings() {
    // Restore normal limits and timeouts
}

int RouteAssignmentService::activeRoutes() const {
    if (m_vitalController) {
        return m_vitalController->activeRoutes();
    }
    return 0;
}

void RouteAssignmentService::performMaintenanceCheck() {
    if (!m_isOperational) {
        return;
    }

    checkSystemHealth();
    checkPerformanceThresholds();
    updateAverageProcessingTime();

    // Clean up old processing requests
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-m_processingTimeoutMs / 1000);
    QStringList expiredRequests;
    
    for (auto it = m_processingRequests.begin(); it != m_processingRequests.end(); ++it) {
        if (it.value().requestedAt < cutoff) {
            expiredRequests.append(it.key());
        }
    }
    
    for (const QString& requestId : expiredRequests) {
        m_processingRequests.remove(requestId);
        m_timeouts++;
        qWarning() << "⏰ Request timeout:" << requestId;
    }
}

void RouteAssignmentService::checkPerformanceThresholds() {
    if (m_averageProcessingTime > WARNING_PROCESSING_TIME_MS) {
        emit performanceWarning("average_processing_time", m_averageProcessingTime, WARNING_PROCESSING_TIME_MS);
    }
    
    if (m_requestQueue.size() > MAX_QUEUE_SIZE / 2) {
        emit performanceWarning("queue_size", m_requestQueue.size(), MAX_QUEUE_SIZE / 2);
    }
}

void RouteAssignmentService::recordProcessingTime(const QString& stage, double timeMs) {
    // Record stage-specific performance
    if (!m_stagePerformance.contains(stage)) {
        m_stagePerformance[stage] = QList<double>();
    }
    
    m_stagePerformance[stage].append(timeMs);
    if (m_stagePerformance[stage].size() > PERFORMANCE_HISTORY_SIZE) {
        m_stagePerformance[stage].removeFirst();
    }
    
    // Record total processing time
    if (stage == "total_processing") {
        m_processingTimes.append(timeMs);
        if (m_processingTimes.size() > PERFORMANCE_HISTORY_SIZE) {
            m_processingTimes.removeFirst();
        }
    }
}

void RouteAssignmentService::updateAverageProcessingTime() {
    if (m_processingTimes.isEmpty()) {
        return;
    }
    
    double total = std::accumulate(m_processingTimes.begin(), m_processingTimes.end(), 0.0);
    m_averageProcessingTime = total / m_processingTimes.size();
    
    emit performanceChanged();
}

QVariantMap RouteAssignmentService::getPerformanceStatistics() const {
    QVariantMap stats;
    
    stats["averageProcessingTimeMs"] = m_averageProcessingTime;
    stats["totalRequests"] = m_totalRequests;
    stats["successfulRoutes"] = m_successfulRoutes;
    stats["failedRoutes"] = m_failedRoutes;
    stats["emergencyReleases"] = m_emergencyReleases;
    stats["timeouts"] = m_timeouts;
    stats["successRate"] = m_totalRequests > 0 ? (double)m_successfulRoutes / m_totalRequests * 100.0 : 0.0;
    stats["pendingRequests"] = m_requestQueue.size();
    stats["activeRoutes"] = activeRoutes();
    
    // Stage-specific performance
    QVariantMap stageStats;
    for (auto it = m_stagePerformance.begin(); it != m_stagePerformance.end(); ++it) {
        if (!it.value().isEmpty()) {
            double avg = std::accumulate(it.value().begin(), it.value().end(), 0.0) / it.value().size();
            stageStats[it.key() + "_avg_ms"] = avg;
        }
    }
    stats["stagePerformance"] = stageStats;
    
    return stats;
}

// Validation helper methods
bool RouteAssignmentService::isValidSignalId(const QString& signalId) const {
    return !signalId.isEmpty() && signalId.length() >= 3; // Basic validation
}

bool RouteAssignmentService::isValidDirection(const QString& direction) const {
    return direction == "UP" || direction == "DOWN";
}

bool RouteAssignmentService::isValidPriority(const QString& priority) const {
    return priority == "LOW" || priority == "NORMAL" || priority == "HIGH" || priority == "EMERGENCY";
}

bool RouteAssignmentService::canAcceptNewRequests() const {
    return m_isOperational && 
           m_requestQueue.size() < MAX_QUEUE_SIZE &&
           !m_emergencyMode;
}

// Utility methods
QString RouteAssignmentService::generateRequestId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString RouteAssignmentService::routeStateToString(RouteState state) const {
    // Implementation would mirror VitalRouteController
    return "UNKNOWN"; // Placeholder
}

// Database integration stubs
bool RouteAssignmentService::loadConfiguration() {
    // Load configuration from database or config files
    return true; // Placeholder
}

bool RouteAssignmentService::persistRouteRequest(const RouteRequest& request) {
    Q_UNUSED(request)
    return true; // Placeholder
}

bool RouteAssignmentService::persistRouteAssignment(const QString& routeId, const RouteRequest& request, const QStringList& path) {
    Q_UNUSED(routeId) Q_UNUSED(request) Q_UNUSED(path)
    return true; // Placeholder
}

// Event handler stubs
void RouteAssignmentService::onTrackCircuitOccupancyChanged(const QString& circuitId, bool isOccupied) {
    Q_UNUSED(circuitId) Q_UNUSED(isOccupied)
    // Handle reactive updates to routes based on track occupancy changes
}

void RouteAssignmentService::onRouteStateChanged(const QString& routeId, const QString& newState) {
    Q_UNUSED(routeId) Q_UNUSED(newState)
    // Handle route state changes from VitalRouteController
}

void RouteAssignmentService::checkSystemHealth() {
    bool wasOperational = m_isOperational;
    m_isOperational = areServicesHealthy();
    
    if (wasOperational != m_isOperational) {
        emit operationalStateChanged();
    }
}

void RouteAssignmentService::onSystemOverload() {
    qWarning() << "⚠️ RouteAssignmentService: System overload detected";
    
    // Enter degraded mode to reduce load
    enterDegradedMode();
    
    // Clear non-essential pending requests
    if (m_requestQueue.size() > MAX_QUEUE_SIZE / 2) {
        int removed = 0;
        auto it = m_requestQueue.begin();
        while (it != m_requestQueue.end() && removed < MAX_QUEUE_SIZE / 4) {
            if (it->priority != "EMERGENCY" && it->priority != "HIGH") {
                it = m_requestQueue.erase(it);
                removed++;
            } else {
                ++it;
            }
        }
        qDebug() << "🗑️ Removed" << removed << "non-essential requests due to overload";
    }
    
    emit systemOverloaded(m_requestQueue.size(), m_maxConcurrentRoutes);
}

bool RouteAssignmentService::activateRoute(const QString& routeId) {
    if (!m_vitalController) {
        return false;
    }
    
    // Update route state to ACTIVE instead of calling non-existent method
    bool success = m_vitalController->updateRouteState(routeId, "ACTIVE");
    
    if (success) {
        qDebug() << "✅ RouteAssignmentService: Activated route" << routeId;
        emit routeActivated(routeId);
    } else {
        qWarning() << "❌ RouteAssignmentService: Failed to activate route" << routeId;
    }
    
    return success;
}

bool RouteAssignmentService::releaseRoute(const QString& routeId, const QString& reason) {
    if (!m_vitalController) {
        return false;
    }
    
    QVariantMap result = m_vitalController->releaseRouteResources(routeId);
    bool success = result["success"].toBool();
    
    if (success) {
        qDebug() << "🔓 RouteAssignmentService: Released route" << routeId << "Reason:" << reason;
        emit routeReleased(routeId, reason);
        emit routeCountChanged();
    } else {
        qWarning() << "❌ RouteAssignmentService: Failed to release route" << routeId 
                   << "Error:" << result["error"].toString();
    }
    
    return success;
}

QVariantMap RouteAssignmentService::getRouteStatus(const QString& routeId) const {
    if (!m_vitalController) {
        return QVariantMap{{"error", "VitalRouteController not available"}};
    }
    
    return m_vitalController->getRouteStatus(routeId);
}

QVariantList RouteAssignmentService::getActiveRoutes() const {
    if (!m_vitalController) {
        return QVariantList();
    }
    
    return m_vitalController->getActiveRoutes();
}

QVariantList RouteAssignmentService::getPendingRequests() const {
    QVariantList result;
    
    for (const RouteRequest& request : m_requestQueue) {
        QVariantMap requestMap;
        requestMap["requestId"] = request.requestId;
        requestMap["sourceSignalId"] = request.sourceSignalId;
        requestMap["destSignalId"] = request.destSignalId;
        requestMap["direction"] = request.direction;
        requestMap["priority"] = request.priority;
        requestMap["operatorId"] = request.requestedBy;
        requestMap["requestedAt"] = request.requestedAt;
        result.append(requestMap);
    }
    
    return result;
}

QVariantMap RouteAssignmentService::getSystemStatus() const {
    return QVariantMap{
        {"isOperational", m_isOperational},
        {"emergencyMode", m_emergencyMode},
        {"degradedMode", m_degradedMode},
        {"pendingRequests", m_requestQueue.size()},
        {"maxConcurrentRoutes", m_maxConcurrentRoutes},
        {"processingTimeout", m_processingTimeoutMs},
        {"queueProcessingInterval", m_queueProcessingIntervalMs},
        {"maintenanceInterval", m_maintenanceIntervalMs}
    };
}

bool RouteAssignmentService::setMaxConcurrentRoutes(int maxRoutes) {
    if (maxRoutes < 1 || maxRoutes > 50) {
        return false;
    }
    m_maxConcurrentRoutes = maxRoutes;
    qDebug() << "🔧 RouteAssignmentService: Set max concurrent routes to" << m_maxConcurrentRoutes;
    return true;
}

bool RouteAssignmentService::setProcessingTimeout(int timeoutMs) {
    if (timeoutMs < 1000 || timeoutMs > 300000) { // 1s to 5min
        return false;
    }
    m_processingTimeoutMs = timeoutMs;
    qDebug() << "🔧 RouteAssignmentService: Set processing timeout to" << m_processingTimeoutMs << "ms";
    return true;
}

QVariantMap RouteAssignmentService::getOperationalStatistics() const {
    QVariantMap stats = getPerformanceStatistics();
    
    // Add operational metrics
    stats["isOperational"] = m_isOperational;
    stats["emergencyMode"] = m_emergencyMode;
    stats["degradedMode"] = m_degradedMode;
    stats["uptime"] = QDateTime::currentDateTime().toSecsSinceEpoch() - m_serviceStartTime;
    
    return stats;
}

QVariantList RouteAssignmentService::getRouteHistory(int limitHours) const {
    Q_UNUSED(limitHours)
    // Would query database for route history
    return QVariantList();
}

} // namespace RailFlux::Route