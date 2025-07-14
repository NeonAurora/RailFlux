#include "InterlockingService.h"
#include "SignalBranch.h"
#include "TrackCircuitBranch.h"
#include "PointMachineBranch.h"
#include "../database/DatabaseManager.h"
#include <QDebug>

// ValidationResult Implementation
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

// InterlockingService Implementation
InterlockingService::InterlockingService(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent), m_dbManager(dbManager) {

    // Create validation branches
    m_signalBranch = std::make_unique<SignalBranch>(dbManager, this);
    m_trackBranch = std::make_unique<TrackCircuitBranch>(dbManager, this);
    m_pointBranch = std::make_unique<PointMachineBranch>(dbManager, this);

    connect(m_trackBranch.get(), &TrackCircuitBranch::systemFreezeRequired,
            this, &InterlockingService::systemFreezeRequired);
}

InterlockingService::~InterlockingService() {
    // Cleanup handled by smart pointers
}

bool InterlockingService::initialize() {
    if (!m_dbManager || !m_dbManager->isConnected()) {
        qWarning() << "❌ Cannot initialize interlocking: Database not connected";
        return false;
    }

    m_isOperational = true;
    emit operationalStateChanged(m_isOperational);

    qDebug() << "✅ Interlocking service initialized successfully";
    return true;
}

ValidationResult InterlockingService::validateSignalOperation(
    const QString& signalId, const QString& currentAspect,
    const QString& requestedAspect, const QString& operatorId) {

    QElapsedTimer timer;
    timer.start();

    if (!m_isOperational) {
        return ValidationResult::blocked("Interlocking system not operational", "SYSTEM_OFFLINE");
    }

    // Delegate to signal branch
    auto result = m_signalBranch->validateAspectChange(signalId, currentAspect, requestedAspect, operatorId);

    // Record performance
    double responseTime = timer.elapsed();
    {
        std::lock_guard<std::mutex> lock(m_performanceMutex);
        m_responseTimeHistory.push_back(responseTime);
        if (m_responseTimeHistory.size() > MAX_RESPONSE_HISTORY) {
            m_responseTimeHistory.pop_front();
        }
    }

    if (responseTime > TARGET_RESPONSE_TIME_MS) {
        qWarning() << "⚠️ Slow interlocking response:" << responseTime << "ms for signal" << signalId;
    }

    qDebug() << "🚦 Signal validation completed in" << responseTime << "ms:" << result.getReason();

    if (!result.isAllowed()) {
        emit operationBlocked(signalId, result.getReason());
    }

    return result;
}

ValidationResult InterlockingService::validatePointMachineOperation(
    const QString& machineId, const QString& currentPosition,
    const QString& requestedPosition, const QString& operatorId) {

    if (!m_isOperational) {
        return ValidationResult::blocked("Interlocking system not operational", "SYSTEM_OFFLINE");
    }

    return m_pointBranch->validatePositionChange(machineId, currentPosition, requestedPosition, operatorId);
}

ValidationResult InterlockingService::validateTrackAssignment(
    const QString& trackId, bool currentlyAssigned,
    bool requestedAssignment, const QString& operatorId) {

    if (!m_isOperational) {
        return ValidationResult::blocked("Interlocking system not operational", "SYSTEM_OFFLINE");
    }

    return m_trackBranch->validateTrackAssignment(trackId, currentlyAssigned, requestedAssignment, operatorId);
}

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
    // This would query the database for active interlocking rules
    // For now, return a placeholder
    return 0;
}

void InterlockingService::enforceTrackOccupancyInterlocking(const QString& trackId, bool wasOccupied, bool isOccupied) {
    if (!m_isOperational) {
        qWarning() << "⚠️ Cannot enforce track interlocking - system not operational";
        return;
    }

    // ✅ DELEGATE to track branch for enforcement
    m_trackBranch->enforceTrackOccupancyInterlocking(trackId, wasOccupied, isOccupied);
}
