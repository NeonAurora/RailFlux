#pragma once
#include <QObject>
#include "InterlockingService.h"

class DatabaseManager;

class SignalBranch : public QObject {
    Q_OBJECT

public:
    explicit SignalBranch(DatabaseManager* dbManager, QObject* parent = nullptr);

    ValidationResult validateAspectChange(const QString& signalId,
                                          const QString& currentAspect,
                                          const QString& requestedAspect,
                                          const QString& operatorId);

private:
    DatabaseManager* m_dbManager;

    // Rule implementations
    ValidationResult validateBasicTransition(const QString& signalId,
                                             const QString& currentAspect,
                                             const QString& requestedAspect);
    ValidationResult checkTrackProtection(const QString& signalId, const QString& requestedAspect);
    ValidationResult checkInterlockedSignals(const QString& signalId, const QString& requestedAspect);
    ValidationResult checkSignalActive(const QString& signalId);

    // Helper methods
    QStringList getProtectedTracks(const QString& signalId);
    QStringList getInterlockedSignals(const QString& signalId);
    bool isValidAspectTransition(const QString& from, const QString& to);
    int getAspectPrecedence(const QString& aspect);

    enum class InterlockingType {
        OPPOSING_SIGNALS,
        CONFLICTING_ROUTES,
        SEQUENTIAL_DEPENDENCY,
        HOME_STARTER_PAIR
    };

    InterlockingType determineInterlockingType(const QString& signal1Id, const QString& signal2Id);
};
