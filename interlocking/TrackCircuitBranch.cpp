#include "TrackCircuitBranch.h"
#include "../database/DatabaseManager.h"
#include <QDebug>

TrackCircuitBranch::TrackCircuitBranch(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent), m_dbManager(dbManager) {}

ValidationResult TrackCircuitBranch::validateTrackAssignment(
    const QString& trackId, bool currentlyAssigned,
    bool requestedAssignment, const QString& operatorId) {

    qDebug() << "🛤️ Validating track assignment:" << trackId
             << "from" << currentlyAssigned << "to" << requestedAssignment;

    // 1. Basic track validation
    auto existsResult = checkTrackExists(trackId);
    if (!existsResult.isAllowed()) return existsResult;

    auto activeResult = checkTrackActive(trackId);
    if (!activeResult.isAllowed()) return activeResult;

    // 2. Check if change is actually needed
    if (currentlyAssigned == requestedAssignment) {
        return ValidationResult::allowed("No change required - track already in requested state");
    }

    // 3. Occupancy validation
    auto occupancyResult = checkOccupancyStatus(trackId, requestedAssignment);
    if (!occupancyResult.isAllowed()) return occupancyResult;

    // 4. Signal protection validation
    auto signalResult = checkSignalProtection(trackId, requestedAssignment);
    if (!signalResult.isAllowed()) return signalResult;

    // 5. Approach locking validation
    auto approachResult = checkApproachLocking(trackId, requestedAssignment);
    if (!approachResult.isAllowed()) return approachResult;

    // 6. Route integrity validation
    auto routeResult = checkRouteIntegrity(trackId, requestedAssignment);
    if (!routeResult.isAllowed()) return routeResult;

    // 7. Adjacent track conflicts
    auto adjacentResult = checkAdjacentTrackConflicts(trackId, requestedAssignment);
    if (!adjacentResult.isAllowed()) return adjacentResult;

    // 8. Maintenance mode check
    auto maintenanceResult = checkMaintenanceMode(trackId);
    if (!maintenanceResult.isAllowed()) return maintenanceResult;

    return ValidationResult::allowed("All track assignment validations passed");
}

ValidationResult TrackCircuitBranch::checkTrackExists(const QString& trackId) {
    auto trackData = m_dbManager->getTrackSegmentById(trackId);
    if (trackData.isEmpty()) {
        return ValidationResult::blocked("Track segment not found: " + trackId, "TRACK_NOT_FOUND");
    }
    return ValidationResult::allowed();
}

ValidationResult TrackCircuitBranch::checkTrackActive(const QString& trackId) {
    auto trackState = getTrackState(trackId);
    if (!trackState.isActive) {
        return ValidationResult::blocked("Track segment is not active: " + trackId, "TRACK_INACTIVE");
    }
    return ValidationResult::allowed();
}

ValidationResult TrackCircuitBranch::checkOccupancyStatus(const QString& trackId, bool requestedAssignment) {
    auto trackState = getTrackState(trackId);

    // Cannot assign an occupied track
    if (requestedAssignment && trackState.isOccupied) {
        return ValidationResult::blocked(
            QString("Cannot assign track %1: occupied by %2")
                .arg(trackId, trackState.occupiedBy),
            "TRACK_OCCUPIED"
            );
    }

    // Cannot unassign a track that's still occupied
    if (!requestedAssignment && trackState.isOccupied) {
        return ValidationResult::blocked(
            QString("Cannot unassign track %1: still occupied by %2")
                .arg(trackId, trackState.occupiedBy),
            "TRACK_STILL_OCCUPIED"
            );
    }

    return ValidationResult::allowed();
}

ValidationResult TrackCircuitBranch::checkSignalProtection(const QString& trackId, bool requestedAssignment) {
    QStringList protectingSignals = getProtectingSignals(trackId);

    // If assigning track, ensure protecting signals are at safe aspects
    if (requestedAssignment && !protectingSignals.isEmpty()) {
        if (!areProtectingSignalsAtRed(protectingSignals)) {
            return ValidationResult::blocked(
                QString("Cannot assign track %1: protecting signals not at safe aspects")
                    .arg(trackId),
                "PROTECTING_SIGNALS_NOT_SAFE"
                );
        }
    }

    return ValidationResult::allowed();
}

ValidationResult TrackCircuitBranch::checkApproachLocking(const QString& trackId, bool requestedAssignment) {
    auto trackState = getTrackState(trackId);

    // Check if track has approach lock
    if (trackState.approachLockingActive) {
        QString lockingSignal = trackState.approachLockedBy;

        if (requestedAssignment) {
            return ValidationResult::blocked(
                       QString("Cannot assign track %1: approach locked by signal %2")
                           .arg(trackId, lockingSignal),
                       "APPROACH_LOCKED"
                       ).addAffectedEntity(lockingSignal);
        }

        // For unassignment, check if locking signal is still cleared
        auto signalData = m_dbManager->getSignalById(lockingSignal);
        if (!signalData.isEmpty()) {
            QString signalAspect = signalData["currentAspect"].toString();
            if (signalAspect != "RED") {
                return ValidationResult::blocked(
                           QString("Cannot unassign track %1: approach lock active from signal %2 showing %3")
                               .arg(trackId, lockingSignal, signalAspect),
                           "APPROACH_LOCK_ACTIVE"
                           ).addAffectedEntity(lockingSignal);
            }
        }
    }

    return ValidationResult::allowed();
}

ValidationResult TrackCircuitBranch::checkRouteIntegrity(const QString& trackId, bool requestedAssignment) {
    if (isPartOfActiveRoute(trackId)) {
        if (!requestedAssignment) {
            return ValidationResult::blocked(
                QString("Cannot unassign track %1: part of active route")
                    .arg(trackId),
                "ACTIVE_ROUTE_MEMBER"
                );
        }
    }

    return ValidationResult::allowed();
}

ValidationResult TrackCircuitBranch::checkAdjacentTrackConflicts(const QString& trackId, bool requestedAssignment) {
    QStringList adjacentTracks = getAdjacentTracks(trackId);
    QStringList conflictingTracks = getConflictingTracks(trackId);

    // Check for conflicts with adjacent tracks
    for (const QString& adjacentTrackId : adjacentTracks) {
        auto adjacentState = getTrackState(adjacentTrackId);

        // Specific conflict rules based on your railway layout
        if (requestedAssignment && adjacentState.isAssigned) {
            // Add specific logic based on your track layout
            // This is a simplified check - you'd implement specific rules
            qDebug() << "⚠️ Adjacent track" << adjacentTrackId << "is also assigned";
        }
    }

    // Check explicitly conflicting tracks
    for (const QString& conflictingTrackId : conflictingTracks) {
        auto conflictState = getTrackState(conflictingTrackId);

        if (requestedAssignment && conflictState.isAssigned) {
            return ValidationResult::blocked(
                       QString("Cannot assign track %1: conflicts with assigned track %2")
                           .arg(trackId, conflictingTrackId),
                       "CONFLICTING_TRACK_ASSIGNED"
                       ).addAffectedEntity(conflictingTrackId);
        }
    }

    return ValidationResult::allowed();
}

ValidationResult TrackCircuitBranch::checkMaintenanceMode(const QString& trackId) {
    // Check if track is in maintenance mode
    // This would be implemented based on your maintenance system
    // For now, return allowed
    return ValidationResult::allowed();
}

// Helper method implementations
TrackCircuitBranch::TrackState TrackCircuitBranch::getTrackState(const QString& trackId) {
    TrackState state;
    auto trackData = m_dbManager->getTrackSegmentById(trackId);

    if (!trackData.isEmpty()) {
        state.isOccupied = trackData["occupied"].toBool();
        state.isAssigned = trackData["assigned"].toBool();
        state.isActive = trackData["isActive"].toBool();
        state.occupiedBy = trackData["occupiedBy"].toString();
        // Note: You'll need to add these fields to your database schema
        // state.approachLockingActive = trackData["approachLockingActive"].toBool();
        // state.approachLockedBy = trackData["approachLockedBy"].toString();

        // For now, default values:
        state.approachLockingActive = false;
        state.approachLockedBy = "";
    }

    return state;
}

QStringList TrackCircuitBranch::getProtectingSignals(const QString& trackId) {
    // Query signals that protect this track
    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare("SELECT signal_id FROM railway_control.signal_track_protection WHERE protected_track_id = ? AND is_active = TRUE");
    query.addBindValue(trackId);

    QStringList signalList;
    if (query.exec()) {
        while (query.next()) {
            signalList.append(query.value(0).toString());
        }
    }

    return signalList;
}

QStringList TrackCircuitBranch::getAdjacentTracks(const QString& trackId) {
    // This would be implemented based on your track topology
    // For now, return empty list
    // You could implement this by:
    // 1. Analyzing track coordinates to find adjacent segments
    // 2. Using a track topology table
    // 3. Using point machine connections
    return QStringList();
}

QStringList TrackCircuitBranch::getConflictingTracks(const QString& trackId) {
    // Get explicitly conflicting tracks from database
    auto trackData = m_dbManager->getTrackSegmentById(trackId);
    if (!trackData.isEmpty()) {
        // You'll need to add this field to your database schema
        // return trackData["conflictingTracks"].toStringList();
    }
    return QStringList();
}

bool TrackCircuitBranch::isPartOfActiveRoute(const QString& trackId) {
    // Check if track is part of any active route
    // This would query your routes table when implemented
    return false;
}

bool TrackCircuitBranch::areProtectingSignalsAtRed(const QStringList& signalIds) {
    for (const QString& signalId : signalIds) {
        auto signalData = m_dbManager->getSignalById(signalId);
        if (!signalData.isEmpty()) {
            QString aspect = signalData["currentAspect"].toString();
            if (aspect != "RED") {
                qDebug() << "⚠️ Protecting signal" << signalId << "shows" << aspect << "(not RED)";
                return false;
            }
        }
    }
    return true;
}
