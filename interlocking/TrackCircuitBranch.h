#ifndef TRACKCIRCUITBRANCH_H
#define TRACKCIRCUITBRANCH_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSqlQuery>
#include <QDateTime>
#include "InterlockingService.h"  // ✅ Use existing ValidationResult

class DatabaseManager;

class TrackCircuitBranch : public QObject
{
    Q_OBJECT

public:
    explicit TrackCircuitBranch(DatabaseManager* dbManager, QObject* parent = nullptr);

    // ✅ MAIN FUNCTION: Reactive enforcement when track becomes occupied
    void enforceTrackOccupancyInterlocking(const QString& trackSectionId, bool wasOccupied, bool isOccupied);

    // ✅ UTILITY: Basic track section checks (for safety verification)
    ValidationResult checkTrackSectionExists(const QString& trackSectionId);
    ValidationResult checkTrackSectionActive(const QString& trackSectionId);

signals:
    void systemFreezeRequired(const QString& trackSectionId, const QString& reason, const QString& details);
    void automaticInterlockingCompleted(const QString& trackSectionId, const QStringList& affectedSignals);
    void interlockingFailure(const QString& trackSectionId, const QString& failedSignals, const QString& error);

private:
    DatabaseManager* m_dbManager;

    // ✅ TRACK SECTION STATE: Simplified structure for hardware-based occupancy
    struct TrackSectionState {
        bool isOccupied;
        bool isAssigned;
        bool isActive;
        QString occupiedBy;
        QString trackType;
        QStringList protectingSignals;
    };

    // ✅ CORE METHODS: Track section state and protection
    TrackSectionState getTrackSectionState(const QString& trackSectionId);
    QStringList getProtectingSignalsFromBothSources(const QString& trackSectionId);
    QStringList getProtectingSignalsFromDatabase(const QString& trackSectionId);
    QStringList getProtectingSignalsFromTrackData(const QString& trackSectionId);

    // ✅ ENFORCEMENT METHODS: Automatic signal control
    bool enforceSignalToRed(const QString& signalId, const QString& reason);
    bool enforceMultipleSignalsToRed(const QStringList& signalIds, const QString& reason);
    bool verifySignalIsRed(const QString& signalId);

    // ✅ FAILURE HANDLING: Critical safety system failures
    void handleInterlockingFailure(const QString& trackSectionId, const QString& failedSignals, const QString& error);
    void logCriticalFailure(const QString& trackSectionId, const QString& details);
    void emitSystemFreeze(const QString& trackSectionId, const QString& reason, const QString& details);

    // ✅ UTILITY METHODS: Safety checks
    bool areAllSignalsAtRed(const QStringList& signalIds);
    QString formatFailureDetails(const QString& trackSectionId, const QStringList& failedSignals, const QString& error);
};

#endif // TRACKCIRCUITBRANCH_H
