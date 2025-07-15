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

void TrackCircuitBranch::enforceTrackOccupancyInterlocking(
    const QString& trackSectionId, bool wasOccupied, bool isOccupied) {

    // ✅ SAFETY: Only react to critical transition (track becoming occupied)
    if (wasOccupied || !isOccupied) {
        qDebug() << "🟢 No interlocking action needed for track section" << trackSectionId
                 << "- transition:" << wasOccupied << "→" << isOccupied;
        return;
    }

    qDebug() << "🚨 AUTOMATIC INTERLOCKING TRIGGERED: Track section" << trackSectionId
             << "became occupied - enforcing signal protection";

    // ✅ SAFETY: Verify track section exists and is operational
    auto existsResult = checkTrackSectionExists(trackSectionId);
    if (!existsResult.isAllowed()) {
        qCritical() << "🚨 CRITICAL: Track section" << trackSectionId << "not found during interlocking enforcement!";
        handleInterlockingFailure(trackSectionId, "N/A", "Track section not found: " + existsResult.getReason());
        return;
    }

    auto activeResult = checkTrackSectionActive(trackSectionId);
    if (!activeResult.isAllowed()) {
        qWarning() << "⚠️ Track section" << trackSectionId << "is not active - skipping interlocking enforcement";
        return;
    }

    // ✅ SAFETY: Get protecting signals from multiple sources for redundancy
    QStringList protectingSignals = getProtectingSignalsFromBothSources(trackSectionId);

    if (protectingSignals.isEmpty()) {
        qWarning() << "⚠️ SAFETY WARNING: No protecting signals found for occupied track section" << trackSectionId;
        qWarning() << "⚠️ This could indicate a configuration error or unprotected track section";
        return;
    }

    qDebug() << "🔒 ENFORCING PROTECTION: Setting" << protectingSignals.size()
             << "protecting signals to RED for track section" << trackSectionId;
    qDebug() << "🔒 Protecting signals:" << protectingSignals;

    // ✅ SAFETY: Force all protecting signals to RED - no validation, just enforce
    bool allSucceeded = enforceMultipleSignalsToRed(protectingSignals,
                                                    QString("AUTOMATIC: Track section %1 occupied").arg(trackSectionId));

    if (allSucceeded) {
        qDebug() << "✅ AUTOMATIC INTERLOCKING SUCCESSFUL: All protecting signals set to RED for track section" << trackSectionId;
        emit automaticInterlockingCompleted(trackSectionId, protectingSignals);
    } else {
        qCritical() << "🚨 AUTOMATIC INTERLOCKING FAILED for track section" << trackSectionId;
        // handleInterlockingFailure is called within enforceMultipleSignalsToRed
    }
}

// ============================================================================
// ✅ TRACK SECTION VALIDATION METHODS
// ============================================================================

ValidationResult TrackCircuitBranch::checkTrackSectionExists(const QString& trackSectionId) {
    auto trackData = m_dbManager->getTrackSegmentById(trackSectionId);  // ✅ Use existing method name
    if (trackData.isEmpty()) {
        return ValidationResult::blocked("Track section not found: " + trackSectionId, "TRACK_SECTION_NOT_FOUND");
    }
    return ValidationResult::allowed("Track section exists");
}

ValidationResult TrackCircuitBranch::checkTrackSectionActive(const QString& trackSectionId) {
    auto trackState = getTrackSectionState(trackSectionId);
    if (!trackState.isActive) {
        return ValidationResult::blocked("Track section is not active: " + trackSectionId, "TRACK_SECTION_INACTIVE");
    }
    return ValidationResult::allowed("Track section is active");
}

// ============================================================================
// ✅ TRACK SECTION STATE AND PROTECTION METHODS
// ============================================================================

TrackCircuitBranch::TrackSectionState TrackCircuitBranch::getTrackSectionState(const QString& trackSectionId) {
    TrackSectionState state;
    auto trackData = m_dbManager->getTrackSegmentById(trackSectionId);  // ✅ Use existing method name

    if (!trackData.isEmpty()) {
        state.isOccupied = trackData["occupied"].toBool();
        state.isAssigned = trackData["assigned"].toBool();
        state.isActive = trackData["isActive"].toBool();
        state.occupiedBy = trackData["occupiedBy"].toString();
        state.trackType = trackData["trackType"].toString();

        // ✅ Parse protecting signals array from database
        QString protectingSignalsStr = trackData["protectingSignals"].toString();
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

QStringList TrackCircuitBranch::getProtectingSignalsFromBothSources(const QString& trackSectionId) {
    QStringList combinedSignals;

    // ✅ SOURCE 1: signal_track_protection table (explicit protection relationships)
    QStringList fromProtectionTable = getProtectingSignalsFromDatabase(trackSectionId);

    // ✅ SOURCE 2: track_segments.protecting_signals array (configuration data)
    QStringList fromTrackData = getProtectingSignalsFromTrackData(trackSectionId);

    // ✅ SAFETY: Combine both sources and remove duplicates for redundancy
    combinedSignals = fromProtectionTable;
    for (const QString& signal : fromTrackData) {
        if (!combinedSignals.contains(signal.trimmed())) {
            combinedSignals.append(signal.trimmed());
        }
    }

    qDebug() << "🔍 PROTECTING SIGNALS for track section" << trackSectionId << ":";
    qDebug() << "   From protection table:" << fromProtectionTable;
    qDebug() << "   From track data:" << fromTrackData;
    qDebug() << "   Combined list:" << combinedSignals;

    return combinedSignals;
}

QStringList TrackCircuitBranch::getProtectingSignalsFromDatabase(const QString& trackSectionId) {
    if (!m_dbManager) return QStringList();

    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT signal_id FROM railway_control.signal_track_protection WHERE protected_track_id = ? AND is_active = TRUE");
    query.addBindValue(trackSectionId);

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

QStringList TrackCircuitBranch::getProtectingSignalsFromTrackData(const QString& trackSectionId) {
    auto trackState = getTrackSectionState(trackSectionId);
    return trackState.protectingSignals;
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
        QString trackSectionId = reason.contains("Track section") ?
                                     reason.split(" ")[2] : "UNKNOWN"; // Extract track section ID from reason

        qCritical() << "🚨 CRITICAL SAFETY FAILURE: Failed to set signals to RED";
        qCritical() << "🚨 Succeeded signals:" << succeededSignals;
        qCritical() << "🚨 Failed signals:" << failedSignals;

        handleInterlockingFailure(trackSectionId, failedSignals.join(","), "Failed to enforce RED aspect on multiple signals");
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

void TrackCircuitBranch::handleInterlockingFailure(const QString& trackSectionId, const QString& failedSignals, const QString& error) {
    QString details = formatFailureDetails(trackSectionId, failedSignals.split(","), error);

    logCriticalFailure(trackSectionId, details);
    emitSystemFreeze(trackSectionId, "Failed to enforce signal protection for occupied track section", details);

    // ✅ EMIT specific interlocking failure signal
    emit interlockingFailure(trackSectionId, failedSignals, error);
}

void TrackCircuitBranch::logCriticalFailure(const QString& trackSectionId, const QString& details) {
    qCritical() << "🚨🚨🚨 CRITICAL INTERLOCKING SYSTEM FAILURE 🚨🚨🚨";
    qCritical() << "Track Section ID:" << trackSectionId;
    qCritical() << "Failure Details:" << details;
    qCritical() << "Timestamp:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    qCritical() << "Thread:" << QThread::currentThread();
    qCritical() << "🚨 IMMEDIATE MANUAL INTERVENTION REQUIRED 🚨";
}

void TrackCircuitBranch::emitSystemFreeze(const QString& trackSectionId, const QString& reason, const QString& details) {
    qCritical() << "🚨 EMITTING SYSTEM FREEZE SIGNAL for track section" << trackSectionId;
    emit systemFreezeRequired(trackSectionId, reason, details);
}

QString TrackCircuitBranch::formatFailureDetails(const QString& trackSectionId, const QStringList& failedSignals, const QString& error) {
    return QString("Track Section: %1, Failed Signals: %2, Error: %3, Time: %4")
    .arg(trackSectionId)
        .arg(failedSignals.join(", "))
        .arg(error)
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
}
