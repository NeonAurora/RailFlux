#pragma once
#include <QObject>
#include <QDateTime>
#include "InterlockingService.h"

class DatabaseManager;

class PointMachineBranch : public QObject {
    Q_OBJECT

public:
    explicit PointMachineBranch(DatabaseManager* dbManager, QObject* parent = nullptr);

    ValidationResult validatePositionChange(const QString& machineId,
                                            const QString& currentPosition,
                                            const QString& requestedPosition,
                                            const QString& operatorId);

private:
    DatabaseManager* m_dbManager;

    // Core validation rules
    ValidationResult checkPointMachineExists(const QString& machineId);
    ValidationResult checkPointMachineActive(const QString& machineId);
    ValidationResult checkOperationalStatus(const QString& machineId);
    ValidationResult checkLockingStatus(const QString& machineId);
    ValidationResult checkProtectingSignals(const QString& machineId, const QString& requestedPosition);
    ValidationResult checkTrackOccupancy(const QString& machineId, const QString& requestedPosition);
    ValidationResult checkRouteConflicts(const QString& machineId, const QString& requestedPosition);
    ValidationResult checkTimeLocking(const QString& machineId);
    ValidationResult checkDetectionLocking(const QString& machineId);
    ValidationResult checkConflictingPoints(const QString& machineId, const QString& requestedPosition);

    // Helper methods
    QStringList getProtectingSignals(const QString& machineId);
    QStringList getAffectedTracks(const QString& machineId, const QString& position);
    QStringList getConflictingPointMachines(const QString& machineId);
    bool areAllProtectingSignalsAtRed(const QStringList& signalIds);
    bool areAffectedTracksClear(const QStringList& trackIds);
    bool isInTransition(const QString& machineId);

    struct PointMachineState {
        QString currentPosition;
        QString operatingStatus;
        bool isLocked;
        bool isActive;
        bool timeLockingActive;
        QDateTime timeLockExpiry;
        QStringList detectionLocks;
    };

    struct RouteConflictInfo {
        bool hasConflict;
        QString conflictingRoute;
        QString conflictReason;
    };

    PointMachineState getPointMachineState(const QString& machineId);
    RouteConflictInfo analyzeRouteImpact(const QString& machineId, const QString& requestedPosition);
};
