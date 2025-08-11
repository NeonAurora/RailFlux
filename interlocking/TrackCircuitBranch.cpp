#include "TrackCircuitBranch.h"
#include "../database/DatabaseManager.h"
#include <QDebug>
#include <QThread>
#include <QDateTime>

TrackCircuitBranch::TrackCircuitBranch(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent), m_dbManager(dbManager) {

    if (!dbManager) {
        qCritical() << "🚨 CRITICAL: TrackCircuitBranch initialized with null DatabaseManager!";
    }

    qDebug() << "✅ TrackCircuitBranch initialized for automatic interlocking enforcement";
}

// ============================================================================
// ✅ MAIN REACTIVE ENFORCEMENT METHOD
// ============================================================================

void TrackCircuitBranch::enforceTrackSegmentOccupancyInterlocking(
    const QString& trackSegmentId, bool wasOccupied, bool isOccupied) {

    // ✅ SAFETY: Only react to critical transition (trackSegment becoming occupied)
    if (wasOccupied || !isOccupied) {
        qDebug() << "🟢 No interlocking action needed for track segment" << trackSegmentId
                 << "- transition:" << wasOccupied << "→" << isOccupied;
        return;
    }

    qDebug() << "🚨 AUTOMATIC INTERLOCKING TRIGGERED: Track segment" << trackSegmentId
             << "became occupied - enforcing signal protection";

    // ✅ SAFETY: Verify track segment exists and is operational
    auto existsResult = checkTrackSegmentExists(trackSegmentId);
    if (!existsResult.isAllowed()) {
        qCritical() << "🚨 CRITICAL: Track segment" << trackSegmentId << "not found during interlocking enforcement!";
        handleInterlockingFailure(trackSegmentId, "N/A", "Track segment not found: " + existsResult.getReason());
        return;
    }

    auto activeResult = checkTrackSegmentActive(trackSegmentId);
    if (!activeResult.isAllowed()) {
        qWarning() << "⚠️ Track segment" << trackSegmentId << "is not active - skipping interlocking enforcement";
        return;
    }

    // ✅ SAFETY: Get protecting signals from multiple sources for redundancy
    QStringList protectingSignals = getProtectingSignalsFromBothSources(trackSegmentId);

    if (protectingSignals.isEmpty()) {
        qWarning() << "⚠️ SAFETY WARNING: No protecting signals found for occupied track segment" << trackSegmentId;
        qWarning() << "⚠️ This could indicate a configuration error or unprotected track segment";
        return;
    }

    qDebug() << "🔒 ENFORCING PROTECTION: Setting" << protectingSignals.size()
             << "protecting signals to RED for track segment" << trackSegmentId;
    qDebug() << "🔒 Protecting signals:" << protectingSignals;

    // ✅ SAFETY: Force all protecting signals to RED - no validation, just enforce
    bool allSucceeded = enforceMultipleSignalsToRed(protectingSignals,
                                                    QString("AUTOMATIC: Track segment %1 occupied").arg(trackSegmentId));

    if (allSucceeded) {
        qDebug() << "✅ AUTOMATIC INTERLOCKING SUCCESSFUL: All protecting signals set to RED for track segment" << trackSegmentId;
        emit automaticInterlockingCompleted(trackSegmentId, protectingSignals);
    } else {
        qCritical() << "🚨 AUTOMATIC INTERLOCKING FAILED for track segment" << trackSegmentId;
        // handleInterlockingFailure is called within enforceMultipleSignalsToRed
    }
}

// ============================================================================
// ✅ TRACK SEGMENT SEGMENT VALIDATION METHODS
// ============================================================================

ValidationResult TrackCircuitBranch::checkTrackSegmentExists(const QString& trackSegmentId) {
    auto trackSegmentData = m_dbManager->getTrackSegmentById(trackSegmentId);  // ✅ Use existing method name
    if (trackSegmentData.isEmpty()) {
        return ValidationResult::blocked("Track segment not found: " + trackSegmentId, "TRACK_SEGMENT_NOT_FOUND");
    }
    return ValidationResult::allowed("Track segment exists");
}

ValidationResult TrackCircuitBranch::checkTrackSegmentActive(const QString& trackSegmentId) {
    auto trackSegmentState = getTrackSegmentState(trackSegmentId);
    if (!trackSegmentState.isActive) {
        return ValidationResult::blocked("Track segment is not active: " + trackSegmentId, "TRACK_SEGMENT_INACTIVE");
    }
    return ValidationResult::allowed("Track segment is active");
}

// ============================================================================
// ✅ TRACK SEGMENT SEGMENT STATE AND PROTECTION METHODS
// ============================================================================

TrackCircuitBranch::TrackSegmentState TrackCircuitBranch::getTrackSegmentState(const QString& trackSegmentId) {
    TrackSegmentState state;
    auto trackSegmentData = m_dbManager->getTrackSegmentById(trackSegmentId);  // ✅ Use existing method name

    if (!trackSegmentData.isEmpty()) {
        state.isOccupied = trackSegmentData["occupied"].toBool();
        state.isAssigned = trackSegmentData["assigned"].toBool();
        state.isActive = trackSegmentData["isActive"].toBool();
        state.occupiedBy = trackSegmentData["occupiedBy"].toString();
        state.trackSegmentType = trackSegmentData["trackSegmentType"].toString();

        // ✅ Parse protecting signals array from database
        QString protectingSignalsStr = trackSegmentData["protectingSignals"].toString();
        if (!protectingSignalsStr.isEmpty() && protectingSignalsStr != "{}") {
            protectingSignalsStr = protectingSignalsStr.mid(1, protectingSignalsStr.length() - 2); // Remove { }
            state.protectingSignals = protectingSignalsStr.split(",", Qt::SkipEmptyParts);
            for (QString& signal : state.protectingSignals) {
                signal = signal.trimmed();
            }
        }
    }

    return state;
}

QStringList TrackCircuitBranch::getProtectingSignalsFromBothSources(const QString& trackSegmentId) {
    QStringList combinedSignals;

    // ✅ SOURCE 1: signal_track_segment_protection table (explicit protection relationships)
    QStringList fromProtectionTable = getProtectingSignalsFromDatabase(trackSegmentId);

    // ✅ SOURCE 2: track_segments.protecting_signals array (configuration data)
    QStringList fromTrackSegmentData = getProtectingSignalsFromTrackSegmentData(trackSegmentId);

    // ✅ SAFETY: Combine both sources and remove duplicates for redundancy
    combinedSignals = fromProtectionTable;
    for (const QString& signal : fromTrackSegmentData) {
        if (!combinedSignals.contains(signal.trimmed())) {
            combinedSignals.append(signal.trimmed());
        }
    }

    qDebug() << "🔍 PROTECTING SIGNALS for track segment" << trackSegmentId << ":";
    qDebug() << "   From protection table:" << fromProtectionTable;
    qDebug() << "   From trackSegment data:" << fromTrackSegmentData;
    qDebug() << "   Combined list:" << combinedSignals;

    return combinedSignals;
}

QStringList TrackCircuitBranch::getProtectingSignalsFromDatabase(const QString& trackSegmentId) {
    if (!m_dbManager) return QStringList();

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT signal_id FROM railway_control.signal_track_segment_protection WHERE protected_track_segment_id = ? AND is_active = TRUE");
    query.addBindValue(trackSegmentId);

    QStringList signalList;
    if (query.exec()) {
        while (query.next()) {
            signalList.append(query.value(0).toString());
        }
    } else {
        qWarning() << "❌ Failed to query protecting signals from database:" << query.lastError().text();
    }

    return signalList;
}

QStringList TrackCircuitBranch::getProtectingSignalsFromTrackSegmentData(const QString& trackSegmentId) {
    auto trackSegmentState = getTrackSegmentState(trackSegmentId);
    return trackSegmentState.protectingSignals;
}

// ============================================================================
// ✅ SIGNAL ENFORCEMENT METHODS
// ============================================================================

bool TrackCircuitBranch::enforceSignalToRed(const QString& signalId, const QString& reason) {
    qDebug() << "🔒 ENFORCING RED: Signal" << signalId << "Reason:" << reason;

    // ✅ SAFETY: Check if signal is already RED to avoid unnecessary operations
    if (verifySignalIsRed(signalId)) {
        qDebug() << "✅ Signal" << signalId << "already RED - no action needed";
        return true;
    }

    // ✅ FORCE: Use database manager to set signal to RED (bypasses normal validation)
    bool success = m_dbManager->updateSignalAspect(signalId, "RED");

    if (success) {
        qDebug() << "✅ ENFORCED: Signal" << signalId << "set to RED";

        // ✅ VERIFY: Double-check that signal is actually RED
        QThread::msleep(50); // Brief delay to ensure database update is committed
        if (!verifySignalIsRed(signalId)) {
            qCritical() << "🚨 VERIFICATION FAILED: Signal" << signalId << "not confirmed RED after enforcement!";
            return false;
        }
    } else {
        qCritical() << "🚨 ENFORCEMENT FAILED: Could not set signal" << signalId << "to RED";
    }

    return success;
}

bool TrackCircuitBranch::enforceMultipleSignalsToRed(const QStringList& signalIds, const QString& reason) {
    if (signalIds.isEmpty()) {
        qWarning() << "⚠️ No signals to enforce - empty list provided";
        return true;
    }

    bool allSucceeded = true;
    QStringList failedSignals;
    QStringList succeededSignals;

    qDebug() << "🔒 ENFORCING MULTIPLE SIGNALS TO RED:" << signalIds.size() << "signals";

    for (const QString& signalId : signalIds) {
        if (enforceSignalToRed(signalId, reason)) {
            succeededSignals.append(signalId);
        } else {
            allSucceeded = false;
            failedSignals.append(signalId);
        }
    }

    if (!allSucceeded) {
        QString trackSegmentId = reason.contains("Track segment") ?
                                     reason.split(" ")[2] : "UNKNOWN"; // Extract track segment ID from reason

        qCritical() << "🚨 CRITICAL SAFETY FAILURE: Failed to set signals to RED";
        qCritical() << "🚨 Succeeded signals:" << succeededSignals;
        qCritical() << "🚨 Failed signals:" << failedSignals;

        handleInterlockingFailure(trackSegmentId, failedSignals.join(","), "Failed to enforce RED aspect on multiple signals");
    }

    return allSucceeded;
}

bool TrackCircuitBranch::verifySignalIsRed(const QString& signalId) {
    auto signalData = m_dbManager->getSignalById(signalId);
    if (!signalData.isEmpty()) {
        QString currentAspect = signalData["currentAspect"].toString();
        return currentAspect == "RED";
    }

    qWarning() << "❌ Could not verify signal" << signalId << "- signal data not found";
    return false;
}

bool TrackCircuitBranch::areAllSignalsAtRed(const QStringList& signalIds) {
    for (const QString& signalId : signalIds) {
        if (!verifySignalIsRed(signalId)) {
            qDebug() << "⚠️ Signal" << signalId << "is not at RED";
            return false;
        }
    }
    return true;
}

// ============================================================================
// ✅ FAILURE HANDLING METHODS
// ============================================================================

void TrackCircuitBranch::handleInterlockingFailure(const QString& trackSegmentId, const QString& failedSignals, const QString& error) {
    QString details = formatFailureDetails(trackSegmentId, failedSignals.split(","), error);

    logCriticalFailure(trackSegmentId, details);
    emitSystemFreeze(trackSegmentId, "Failed to enforce signal protection for occupied track segment", details);

    // ✅ EMIT specific interlocking failure signal
    emit interlockingFailure(trackSegmentId, failedSignals, error);
}

void TrackCircuitBranch::logCriticalFailure(const QString& trackSegmentId, const QString& details) {
    qCritical() << "🚨🚨🚨 CRITICAL INTERLOCKING SYSTEM FAILURE 🚨🚨🚨";
    qCritical() << "Track Segment Segment ID:" << trackSegmentId;
    qCritical() << "Failure Details:" << details;
    qCritical() << "Timestamp:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    qCritical() << "Thread:" << QThread::currentThread();
    qCritical() << "🚨 IMMEDIATE MANUAL INTERVENTION REQUIRED 🚨";
}

void TrackCircuitBranch::emitSystemFreeze(const QString& trackSegmentId, const QString& reason, const QString& details) {
    qCritical() << "🚨 EMITTING SYSTEM FREEZE SIGNAL for track segment" << trackSegmentId;
    emit systemFreezeRequired(trackSegmentId, reason, details);
}

QString TrackCircuitBranch::formatFailureDetails(const QString& trackSegmentId, const QStringList& failedSignals, const QString& error) {
    return QString("Track Segment Segment: %1, Failed Signals: %2, Error: %3, Time: %4")
    .arg(trackSegmentId)
        .arg(failedSignals.join(", "))
        .arg(error)
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
}
