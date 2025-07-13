#include "PointMachineBranch.h"
#include "../database/DatabaseManager.h"
#include <QDebug>
#include <QDateTime>

PointMachineBranch::PointMachineBranch(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent), m_dbManager(dbManager) {}

ValidationResult PointMachineBranch::validatePositionChange(
    const QString& machineId, const QString& currentPosition,
    const QString& requestedPosition, const QString& operatorId) {

    qDebug() << "🔄 Validating point machine operation:" << machineId
             << "from" << currentPosition << "to" << requestedPosition;

    // 1. Basic point machine validation
    auto existsResult = checkPointMachineExists(machineId);
    if (!existsResult.isAllowed()) return existsResult;

    auto activeResult = checkPointMachineActive(machineId);
    if (!activeResult.isAllowed()) return activeResult;

    // 2. Check if change is actually needed
    if (currentPosition == requestedPosition) {
        return ValidationResult::allowed("No change required - point already in requested position");
    }

    // 3. Operational status validation
    auto operationalResult = checkOperationalStatus(machineId);
    if (!operationalResult.isAllowed()) return operationalResult;

    // 4. Locking status validation
    auto lockingResult = checkLockingStatus(machineId);
    if (!lockingResult.isAllowed()) return lockingResult;

    // 5. Time-based locking validation
    auto timeLockResult = checkTimeLocking(machineId);
    if (!timeLockResult.isAllowed()) return timeLockResult;

    // 6. Detection locking validation
    auto detectionResult = checkDetectionLocking(machineId);
    if (!detectionResult.isAllowed()) return detectionResult;

    // 7. Protecting signals validation
    auto signalResult = checkProtectingSignals(machineId, requestedPosition);
    if (!signalResult.isAllowed()) return signalResult;

    // 8. Track occupancy validation
    auto trackResult = checkTrackOccupancy(machineId, requestedPosition);
    if (!trackResult.isAllowed()) return trackResult;

    // 9. Conflicting point machines validation
    auto conflictResult = checkConflictingPoints(machineId, requestedPosition);
    if (!conflictResult.isAllowed()) return conflictResult;

    // 10. Route conflicts validation
    auto routeResult = checkRouteConflicts(machineId, requestedPosition);
    if (!routeResult.isAllowed()) return routeResult;

    return ValidationResult::allowed("All point machine validations passed");
}

ValidationResult PointMachineBranch::checkPointMachineExists(const QString& machineId) {
    auto pmData = m_dbManager->getPointMachineById(machineId);
    if (pmData.isEmpty()) {
        return ValidationResult::blocked("Point machine not found: " + machineId, "POINT_MACHINE_NOT_FOUND");
    }
    return ValidationResult::allowed();
}

ValidationResult PointMachineBranch::checkPointMachineActive(const QString& machineId) {
    auto pmData = m_dbManager->getPointMachineById(machineId);
    if (!pmData.isEmpty() && !pmData["isActive"].toBool()) {
        return ValidationResult::blocked("Point machine is not active: " + machineId, "POINT_MACHINE_INACTIVE");
    }
    return ValidationResult::allowed();
}

ValidationResult PointMachineBranch::checkOperationalStatus(const QString& machineId) {
    auto pmState = getPointMachineState(machineId);

    if (pmState.operatingStatus == "IN_TRANSITION") {
        return ValidationResult::blocked(
            QString("Point machine %1 is already in transition").arg(machineId),
            "POINT_MACHINE_IN_TRANSITION"
            );
    }

    if (pmState.operatingStatus == "FAILED") {
        return ValidationResult::blocked(
            QString("Point machine %1 has failed status").arg(machineId),
            "POINT_MACHINE_FAILED"
            );
    }

    if (pmState.operatingStatus == "LOCKED_OUT") {
        return ValidationResult::blocked(
            QString("Point machine %1 is locked out").arg(machineId),
            "POINT_MACHINE_LOCKED_OUT"
            );
    }

    return ValidationResult::allowed();
}

ValidationResult PointMachineBranch::checkLockingStatus(const QString& machineId) {
    auto pmState = getPointMachineState(machineId);

    if (pmState.isLocked) {
        return ValidationResult::blocked(
            QString("Point machine %1 is locked").arg(machineId),
            "POINT_MACHINE_LOCKED"
            );
    }

    return ValidationResult::allowed();
}

ValidationResult PointMachineBranch::checkTimeLocking(const QString& machineId) {
    auto pmState = getPointMachineState(machineId);

    if (pmState.timeLockingActive) {
        QDateTime now = QDateTime::currentDateTime();
        if (pmState.timeLockExpiry > now) {
            return ValidationResult::blocked(
                QString("Point machine %1 is time-locked until %2")
                    .arg(machineId, pmState.timeLockExpiry.toString()),
                "POINT_MACHINE_TIME_LOCKED"
                );
        }
    }

    return ValidationResult::allowed();
}

ValidationResult PointMachineBranch::checkDetectionLocking(const QString& machineId) {
    auto pmState = getPointMachineState(machineId);

    // Check if any detection locks are active
    for (const QString& lockingTrackId : pmState.detectionLocks) {
        auto trackData = m_dbManager->getTrackSegmentById(lockingTrackId);
        if (!trackData.isEmpty() && trackData["occupied"].toBool()) {
            return ValidationResult::blocked(
                       QString("Point machine %1 is detection-locked by occupied track %2")
                           .arg(machineId, lockingTrackId),
                       "POINT_MACHINE_DETECTION_LOCKED"
                       ).addAffectedEntity(lockingTrackId);
        }
    }

    return ValidationResult::allowed();
}

ValidationResult PointMachineBranch::checkProtectingSignals(const QString& machineId, const QString& requestedPosition) {
    QStringList protectingSignals = getProtectingSignals(machineId);

    if (!protectingSignals.isEmpty()) {
        if (!areAllProtectingSignalsAtRed(protectingSignals)) {
            QStringList nonRedSignals;
            for (const QString& signalId : protectingSignals) {
                auto signalData = m_dbManager->getSignalById(signalId);
                if (!signalData.isEmpty()) {
                    QString aspect = signalData["currentAspect"].toString();
                    if (aspect != "RED") {
                        nonRedSignals.append(QString("%1(%2)").arg(signalId, aspect));
                    }
                }
            }

            return ValidationResult::blocked(
                QString("Cannot operate point machine %1: protecting signals not at RED: %2")
                    .arg(machineId, nonRedSignals.join(", ")),
                "PROTECTING_SIGNALS_NOT_RED"
                );
        }
    }

    return ValidationResult::allowed();
}

ValidationResult PointMachineBranch::checkTrackOccupancy(const QString& machineId, const QString& requestedPosition) {
    QStringList affectedTracks = getAffectedTracks(machineId, requestedPosition);

    for (const QString& trackId : affectedTracks) {
        auto trackData = m_dbManager->getTrackSegmentById(trackId);
        if (!trackData.isEmpty() && trackData["occupied"].toBool()) {
            return ValidationResult::blocked(
                       QString("Cannot operate point machine %1: affected track %2 is occupied by %3")
                           .arg(machineId, trackId, trackData["occupiedBy"].toString()),
                       "AFFECTED_TRACK_OCCUPIED"
                       ).addAffectedEntity(trackId);
        }
    }

    return ValidationResult::allowed();
}

ValidationResult PointMachineBranch::checkConflictingPoints(const QString& machineId, const QString& requestedPosition) {
    QStringList conflictingMachines = getConflictingPointMachines(machineId);

    for (const QString& conflictingMachineId : conflictingMachines) {
        auto conflictingPMData = m_dbManager->getPointMachineById(conflictingMachineId);
        if (!conflictingPMData.isEmpty()) {
            QString conflictingPosition = conflictingPMData["position"].toString();

            // Implement specific conflict rules based on your layout
            // This is simplified - you'd implement specific geometric conflicts
            if (conflictingPosition != "NORMAL") {
                return ValidationResult::blocked(
                           QString("Cannot operate point machine %1: conflicts with %2 in %3 position")
                               .arg(machineId, conflictingMachineId, conflictingPosition),
                           "CONFLICTING_POINT_MACHINE"
                           ).addAffectedEntity(conflictingMachineId);
            }
        }
    }

    return ValidationResult::allowed();
}

ValidationResult PointMachineBranch::checkRouteConflicts(const QString& machineId, const QString& requestedPosition) {
    auto routeConflict = analyzeRouteImpact(machineId, requestedPosition);

    if (routeConflict.hasConflict) {
        return ValidationResult::blocked(
            QString("Cannot operate point machine %1: %2")
                .arg(machineId, routeConflict.conflictReason),
            "ROUTE_CONFLICT"
            );
    }

    return ValidationResult::allowed();
}

// Helper method implementations
PointMachineBranch::PointMachineState PointMachineBranch::getPointMachineState(const QString& machineId) {
    PointMachineState state;
    auto pmData = m_dbManager->getPointMachineById(machineId);

    if (!pmData.isEmpty()) {
        state.currentPosition = pmData["position"].toString();
        state.operatingStatus = pmData["operatingStatus"].toString();
        state.isActive = pmData["isActive"].toBool();

        // Note: You'll need to add these fields to your database schema
        // state.isLocked = pmData["isLocked"].toBool();
        // state.timeLockingActive = pmData["timeLockingActive"].toBool();
        // state.timeLockExpiry = pmData["timeLockExpiry"].toDateTime();
        // state.detectionLocks = pmData["detectionLocks"].toStringList();

        // For now, default values:
        state.isLocked = false;
        state.timeLockingActive = false;
        state.timeLockExpiry = QDateTime();
        state.detectionLocks = QStringList();
    }

    return state;
}

QStringList PointMachineBranch::getProtectingSignals(const QString& machineId) {
    // Query signals that must be at RED before operating this point machine
    auto pmData = m_dbManager->getPointMachineById(machineId);
    if (!pmData.isEmpty()) {
        // You'll need to add this field to your database schema
        // return pmData["protectedSignals"].toStringList();
    }
    return QStringList();
}

QStringList PointMachineBranch::getAffectedTracks(const QString& machineId, const QString& position) {
    auto pmData = m_dbManager->getPointMachineById(machineId);
    if (!pmData.isEmpty()) {
        // Get track connections from point machine data
        QVariantMap rootTrack = pmData["rootTrack"].toMap();
        QVariantMap normalTrack = pmData["normalTrack"].toMap();
        QVariantMap reverseTrack = pmData["reverseTrack"].toMap();

        QStringList tracks;
        tracks.append(rootTrack["trackId"].toString());

        if (position == "NORMAL") {
            tracks.append(normalTrack["trackId"].toString());
        } else {
            tracks.append(reverseTrack["trackId"].toString());
        }

        return tracks;
    }
    return QStringList();
}

QStringList PointMachineBranch::getConflictingPointMachines(const QString& machineId) {
    // Get explicitly conflicting point machines from database
    auto pmData = m_dbManager->getPointMachineById(machineId);
    if (!pmData.isEmpty()) {
        // You'll need to add this field to your database schema
        // return pmData["conflictingPoints"].toStringList();
    }
    return QStringList();
}

bool PointMachineBranch::areAllProtectingSignalsAtRed(const QStringList& signalIds) {
    for (const QString& signalId : signalIds) {
        auto signalData = m_dbManager->getSignalById(signalId);
        if (!signalData.isEmpty()) {
            QString aspect = signalData["currentAspect"].toString();
            if (aspect != "RED") {
                return false;
            }
        }
    }
    return true;
}

PointMachineBranch::RouteConflictInfo PointMachineBranch::analyzeRouteImpact(
    const QString& machineId, const QString& requestedPosition) {

    RouteConflictInfo info;
    info.hasConflict = false;
    info.conflictingRoute = "";
    info.conflictReason = "";

    // This would analyze if the point machine operation conflicts with active routes
    // For now, return no conflict
    // You would implement this when you add route management

    return info;
}
