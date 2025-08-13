#include "InterlockingService.h"
#include "SignalBranch.h"
#include "TrackCircuitBranch.h"
#include "PointMachineBranch.h"
#include "../database/DatabaseManager.h"
#include <QDebug>

// ============================================================================
// ✅ ValidationResult Implementation
// ============================================================================

ValidationResult::ValidationResult(Status status, const QString& reason, Severity severity)
    : m_status(status), m_severity(severity), m_reason(reason)
    , m_evaluationTime(QDateTime::currentDateTime()) {}

ValidationResult ValidationResult::allowed(const QString& reason) {
    return ValidationResult(Status::ALLOWED, reason, Severity::INFO);
}

ValidationResult ValidationResult::blocked(const QString& reason, const QString& ruleId) {
    auto result = ValidationResult(Status::BLOCKED, reason, Severity::CRITICAL);
    if (!ruleId.isEmpty()) result.setRuleId(ruleId);
    return result;
}

QVariantMap ValidationResult::toVariantMap() const {
    QVariantMap map;
    map["isAllowed"] = isAllowed();
    map["reason"] = m_reason;
    map["ruleId"] = m_ruleId;
    map["severity"] = static_cast<int>(m_severity);
    map["affectedEntities"] = m_affectedEntities;
    map["evaluationTime"] = m_evaluationTime;
    return map;
}

// ============================================================================
// ✅ InterlockingService Implementation
// ============================================================================

InterlockingService::InterlockingService(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent), m_dbManager(dbManager) {

    if (!dbManager) {
        qCritical() << "🚨 CRITICAL: InterlockingService initialized with null DatabaseManager!";
        return;
    }

    // ✅ CREATE VALIDATION BRANCHES
    m_signalBranch = std::make_unique<SignalBranch>(dbManager, this);
    m_trackSegmentBranch = std::make_unique<TrackCircuitBranch>(dbManager, this);
    m_pointBranch = std::make_unique<PointMachineBranch>(dbManager, this);

    // ✅ CONNECT SAFETY SIGNALS: TrackCircuitBranch safety signals
    connect(m_trackSegmentBranch.get(), &TrackCircuitBranch::systemFreezeRequired,
            this, &InterlockingService::systemFreezeRequired);

    connect(m_trackSegmentBranch.get(), &TrackCircuitBranch::interlockingFailure,
            this, &InterlockingService::handleInterlockingFailure);

    connect(m_trackSegmentBranch.get(), &TrackCircuitBranch::automaticInterlockingCompleted,
            this, [this](const QString& trackSegmentId, const QStringList& affectedSignals) {
                qDebug() << "✅ Automatic interlocking completed for trackSegment section" << trackSegmentId;
                emit automaticProtectionActivated(trackSegmentId,
                                                  QString("Automatic signal protection activated for %1 signals").arg(affectedSignals.size()));
            });

    qDebug() << "✅ InterlockingService initialized with all branches connected";
}

InterlockingService::~InterlockingService() {
    qDebug() << "🧹 InterlockingService destructor called";
    // ✅ Cleanup handled by smart pointers
}

bool InterlockingService::initialize() {
    if (!m_dbManager || !m_dbManager->isConnected()) {
        qWarning() << "❌ Cannot initialize interlocking: Database not connected";
        m_isOperational = false;
        emit operationalStateChanged(m_isOperational);
        return false;
    }

    m_isOperational = true;
    emit operationalStateChanged(m_isOperational);

    qDebug() << "✅ Interlocking service initialized and operational";
    return true;
}

// ============================================================================
// ✅ VALIDATION METHODS: For operator-initiated actions only
// ============================================================================

ValidationResult InterlockingService::validateMainSignalOperation(
    const QString& signalId, const QString& currentAspect,
    const QString& requestedAspect, const QString& operatorId) {

    QElapsedTimer timer;
    timer.start();

    if (!m_isOperational) {
        return ValidationResult::blocked("Interlocking system not operational", "SYSTEM_OFFLINE");
    }

    if (!m_signalBranch) {
        qCritical() << "🚨 CRITICAL: SignalBranch not initialized!";
        return ValidationResult::blocked("Signal validation not available", "SIGNAL_BRANCH_MISSING");
    }

    // ✅ DELEGATE TO SIGNAL BRANCH
    auto result = m_signalBranch->validatMainAspectChange(signalId, currentAspect, requestedAspect, operatorId);

    // ✅ RECORD PERFORMANCE
    double responseTime = timer.elapsed();
    recordResponseTime(responseTime);

    if (responseTime > TARGET_RESPONSE_TIME_MS) {
        logPerformanceWarning("Signal validation", responseTime);
    }

    qDebug() << "🚦 Signal validation completed in" << responseTime << "ms:" << result.getReason();

    if (!result.isAllowed()) {
        emit operationBlocked(signalId, result.getReason());
    }

    return result;
}

// ✅ SIMPLIFIED: InterlockingService delegates to SignalBranch
ValidationResult InterlockingService::validateSubsidiarySignalOperation(
    const QString& signalId, const QString& aspectType,
    const QString& currentAspect, const QString& requestedAspect,
    const QString& operatorId) {

    QElapsedTimer timer;
    timer.start();

    qDebug() << "🚦🔧 SUBSIDIARY SIGNAL VALIDATION:" << signalId
             << "Type:" << aspectType
             << "Transition:" << currentAspect << "→" << requestedAspect
             << "Operator:" << operatorId;

    // ✅ SYSTEM AVAILABILITY CHECK
    if (!m_isOperational) {
        qWarning() << "❌ Subsidiary signal validation blocked: Interlocking system not operational";
        return ValidationResult::blocked("Interlocking system not operational", "SYSTEM_OFFLINE");
    }

    if (!m_signalBranch) {
        qCritical() << "🚨 CRITICAL: SignalBranch not initialized for subsidiary signal validation!";
        return ValidationResult::blocked("Signal validation not available", "SIGNAL_BRANCH_MISSING");
    }

    // ✅ VALIDATE ASPECT TYPE
    if (aspectType != "CALLING_ON" && aspectType != "LOOP") {
        qWarning() << "❌ Invalid subsidiary aspect type:" << aspectType;
        return ValidationResult::blocked(
            QString("Invalid subsidiary aspect type: %1").arg(aspectType),
            "INVALID_ASPECT_TYPE");
    }

    // ✅ DELEGATE TO SIGNAL BRANCH: Same pattern as main signals
    auto result = m_signalBranch->validateSubsidiaryAspectChange(
        signalId, aspectType, currentAspect, requestedAspect, operatorId);

    // ✅ PERFORMANCE MONITORING
    double responseTime = timer.elapsed();
    recordResponseTime(responseTime);

    if (responseTime > TARGET_RESPONSE_TIME_MS) {
        logPerformanceWarning(QString("Subsidiary signal validation (%1)").arg(aspectType), responseTime);
    }

    qDebug() << "🚦🔧 Subsidiary signal validation completed in" << responseTime << "ms:"
             << aspectType << result.getReason();

    // ✅ EMIT BLOCKING SIGNAL IF NECESSARY
    if (!result.isAllowed()) {
        emit operationBlocked(signalId, result.getReason());
        qDebug() << "🚨 Subsidiary signal operation blocked:" << signalId << aspectType << result.getReason();
    }

    return result;
}

ValidationResult InterlockingService::validatePointMachineOperation(
    const QString& machineId, const QString& currentPosition,
    const QString& requestedPosition, const QString& operatorId) {

    QElapsedTimer timer;
    timer.start();

    if (!m_isOperational) {
        return ValidationResult::blocked("Interlocking system not operational", "SYSTEM_OFFLINE");
    }

    if (!m_pointBranch) {
        qCritical() << "🚨 CRITICAL: PointMachineBranch not initialized!";
        return ValidationResult::blocked("Point machine validation not available", "POINT_BRANCH_MISSING");
    }

    // ✅ DELEGATE TO POINT MACHINE BRANCH
    auto result = m_pointBranch->validatePositionChange(machineId, currentPosition, requestedPosition, operatorId);

    // ✅ RECORD PERFORMANCE
    double responseTime = timer.elapsed();
    recordResponseTime(responseTime);

    qDebug() << "🔄 Point machine validation completed in" << responseTime << "ms:" << result.getReason();

    if (!result.isAllowed()) {
        emit operationBlocked(machineId, result.getReason());
    }

    return result;
}

// ============================================================================
// ✅ REACTIVE INTERLOCKING: Hardware-driven trackSegment occupancy changes
// ============================================================================

void InterlockingService::reactToTrackSegmentOccupancyChange(
    const QString& trackSegmentId, bool wasOccupied, bool isOccupied) {

    if (!m_isOperational) {
        qCritical() << "🚨 CRITICAL: Interlocking system offline during trackSegment occupancy change!";
        emit systemFreezeRequired(trackSegmentId, "Interlocking system not operational",
                                  QString("Track Segment occupancy change detected while system offline: %1")
                                      .arg(QDateTime::currentDateTime().toString()));
        return;
    }

    if (!m_trackSegmentBranch) {
        qCritical() << "🚨 CRITICAL: TrackCircuitBranch not initialized during occupancy change!";
        emit systemFreezeRequired(trackSegmentId, "Track Segment circuit branch not available",
                                  QString("Track Segment occupancy change cannot be processed: %1")
                                      .arg(QDateTime::currentDateTime().toString()));
        return;
    }

    qDebug() << "🎯 REACTIVE INTERLOCKING: Track Segment section" << trackSegmentId
             << "occupancy changed:" << wasOccupied << "→" << isOccupied;

    // ✅ ENFORCE INTERLOCKING: Only when trackSegment becomes occupied (safety-critical transition)
    if (!wasOccupied && isOccupied) {
        qDebug() << "🚨 SAFETY-CRITICAL TRANSITION: Track Segment section" << trackSegmentId << "became occupied";
        m_trackSegmentBranch->enforceTrackSegmentOccupancyInterlocking(trackSegmentId, wasOccupied, isOccupied);
    } else {
        qDebug() << "🟢 Non-critical transition for trackSegment section" << trackSegmentId << "- no interlocking action needed";
    }
}

// ============================================================================
// ✅ PERFORMANCE AND MONITORING METHODS
// ============================================================================

double InterlockingService::getAverageResponseTime() const {
    std::lock_guard<std::mutex> lock(m_performanceMutex);
    if (m_responseTimeHistory.empty()) return 0.0;

    double sum = 0.0;
    for (double time : m_responseTimeHistory) {
        sum += time;
    }
    return sum / m_responseTimeHistory.size();
}

int InterlockingService::getActiveInterlocksCount() const {
    // ✅ FUTURE: This would query the database for active interlocking rules
    // For now, return a placeholder
    return 0;
}

void InterlockingService::recordResponseTime(double responseTimeMs) {
    std::lock_guard<std::mutex> lock(m_performanceMutex);
    m_responseTimeHistory.push_back(responseTimeMs);
    if (m_responseTimeHistory.size() > MAX_RESPONSE_HISTORY) {
        m_responseTimeHistory.pop_front();
    }
    emit performanceChanged();
}

void InterlockingService::logPerformanceWarning(const QString& operation, double responseTimeMs) {
    qWarning() << "⚠️ Slow interlocking response:" << responseTimeMs << "ms for" << operation
               << "(target:" << TARGET_RESPONSE_TIME_MS << "ms)";
}

// ============================================================================
// ✅ FAILURE HANDLING SLOTS
// ============================================================================

void InterlockingService::handleCriticalFailure(const QString& entityId, const QString& reason) {
    qCritical() << "🚨🚨🚨 INTERLOCKING SYSTEM CRITICAL FAILURE 🚨🚨🚨";
    qCritical() << "Entity:" << entityId << "Reason:" << reason;
    qCritical() << "Timestamp:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    // ✅ EMIT SYSTEM FREEZE: Manual intervention required
    emit systemFreezeRequired(entityId, reason,
                              QString("Critical interlocking failure: %1 at %2")
                                  .arg(reason, QDateTime::currentDateTime().toString()));

    // ✅ EMIT SAFETY VIOLATION
    emit criticalSafetyViolation(entityId, reason);

    // ✅ SET SYSTEM NON-OPERATIONAL
    m_isOperational = false;
    emit operationalStateChanged(m_isOperational);
}

void InterlockingService::handleInterlockingFailure(const QString& trackSegmentId, const QString& failedSignals, const QString& error) {
    qCritical() << "🚨 INTERLOCKING ENFORCEMENT FAILURE:";
    qCritical() << "  Track Segment Section:" << trackSegmentId;
    qCritical() << "  Failed Signals:" << failedSignals;
    qCritical() << "  Error:" << error;

    // ✅ TREAT AS CRITICAL FAILURE: This is a safety system failure
    handleCriticalFailure(trackSegmentId, QString("Failed to enforce signal protection: %1").arg(error));
}
