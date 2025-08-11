#pragma once
#include <QObject>
#include <QSqlQuery>
#include "InterlockingService.h"
#include "InterlockingRuleEngine.h"

class DatabaseManager;

class SignalBranch : public QObject {
    Q_OBJECT

public:
    explicit SignalBranch(DatabaseManager* dbManager, QObject* parent = nullptr);

    // ✅ Main validation interface
    ValidationResult validateAspectChange(const QString& signalId,
                                          const QString& currentAspect,
                                          const QString& requestedAspect,
                                          const QString& operatorId);

private:
    // ============================================================================
    // ENUMS AND DATA STRUCTURES
    // ============================================================================

    enum class SignalGroup {
        MAIN_SIGNALS,        // RED, YELLOW, GREEN, SINGLE_YELLOW, DOUBLE_YELLOW
        CALLING_ON,          // WHITE
        LOOP_SIGNALS,        // YELLOW/OFF (part of home signals)
        SHUNT_SIGNALS,       // BLUE (future)
        BLOCK_SIGNALS        // PURPLE (future)
    };

    enum class InterlockingType {
        OPPOSING_SIGNALS,
        CONFLICTING_ROUTES,
        SEQUENTIAL_DEPENDENCY,
        HOME_STARTER_PAIR
    };

    struct SignalCapabilities {
        QStringList supportedAspects;
        SignalGroup primaryGroup;
        bool supportsCallingOn;
        bool supportsLoop;
    };

    struct ProtectedTrackSegmentsValidation {
        bool isValid;
        QStringList protectedTrackSegments;
        QString errorReason;
        QStringList inconsistentSources;
        QStringList occupiedTrackSegments;
    };

    // ============================================================================
    // CORE VALIDATION METHODS
    // ============================================================================

    ValidationResult validateBasicTransition(const QString& signalId,
                                             const QString& currentAspect,
                                             const QString& requestedAspect);

    ValidationResult checkTrackSegmentProtection(const QString& signalId,
                                          const QString& requestedAspect);

    ValidationResult checkInterlockedSignals(const QString& signalId,
                                             const QString& requestedAspect);

    ValidationResult checkSignalActive(const QString& signalId);

    // ============================================================================
    // PROTECTED TRACK SEGMENTS DATA SOURCES (Triple Redundancy)
    // ============================================================================

    ProtectedTrackSegmentsValidation validateProtectedTrackSegments(const QString& signalId);

    QStringList getProtectedTrackSegmentsFromSignalData(const QString& signalId);
    QStringList getProtectedTrackSegmentsFromInterlockingRules(const QString& signalId);
    QStringList getProtectedTrackSegmentsFromProtectionTable(const QString& signalId);

    bool validateTrackSegmentConsistency(const QStringList& fromSignalData,
                                  const QStringList& fromInterlockingRules,
                                  const QStringList& fromProtectionTable,
                                  ProtectedTrackSegmentsValidation& result);

    bool validateTrackSegmentOccupancy(const QStringList& protectedTrackSegments,
                                ProtectedTrackSegmentsValidation& result);

    // ============================================================================
    // SIMPLIFIED ACCESS METHODS (for external use)
    // ============================================================================

    QStringList getProtectedTrackSegments(const QString& signalId);        // ✅ Still needed
    QStringList getInterlockedSignals(const QString& signalId);
    QStringList getSignalCapabilities(const QString& signalId);

    // ============================================================================
    // ASPECT TRANSITION VALIDATION
    // ============================================================================

    bool isValidAspectTransition(const QString& from, const QString& to);
    SignalGroup determineSignalGroup(const QString& aspect);
    bool isDangerousInterGroupTransition(SignalGroup fromGroup, SignalGroup toGroup,
                                         const QString& fromAspect, const QString& toAspect);

    // ============================================================================
    // SIGNAL CAPABILITY VALIDATION
    // ============================================================================

    bool supportsAspect(const QString& signalId, const QString& aspect);
    int getAspectPrecedence(const QString& aspect);
    InterlockingType determineInterlockingType(const QString& signal1Id, const QString& signal2Id);

    // ============================================================================
    // MEMBER VARIABLES
    // ============================================================================

    DatabaseManager* m_dbManager;
    QString m_currentSignalId;  // Context for validation
    std::unique_ptr<InterlockingRuleEngine> m_ruleEngine;
    ValidationResult checkInterlockedSignals(const QString& signalId,
                                             const QString& currentAspect,
                                             const QString& requestedAspect);
};
