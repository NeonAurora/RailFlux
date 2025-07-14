#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QDateTime>
#include <memory>
#include <deque>
#include <mutex>

class DatabaseManager;
class SignalBranch;
class TrackCircuitBranch;
class PointMachineBranch;

class ValidationResult {
    Q_GADGET
    Q_PROPERTY(bool isAllowed READ isAllowed)
    Q_PROPERTY(QString reason READ getReason)
    Q_PROPERTY(QString ruleId READ getRuleId)
    Q_PROPERTY(int severity READ getSeverity)

public:
    enum class Status { ALLOWED, BLOCKED, CONDITIONAL, MANUAL_OVERRIDE };
    enum class Severity { INFO = 0, WARNING = 1, CRITICAL = 2, EMERGENCY = 3 };

private:
    Status m_status = Status::BLOCKED;
    Severity m_severity = Severity::CRITICAL;
    QString m_reason;
    QString m_ruleId;
    QStringList m_affectedEntities;
    QDateTime m_evaluationTime;

public:
    ValidationResult(Status status = Status::BLOCKED, const QString& reason = "Unknown", Severity severity = Severity::CRITICAL);

    // Status checking
    bool isAllowed() const { return m_status == Status::ALLOWED; }
    bool isBlocked() const { return m_status == Status::BLOCKED; }

    // Getters
    QString getReason() const { return m_reason; }
    QString getRuleId() const { return m_ruleId; }
    int getSeverity() const { return static_cast<int>(m_severity); }
    QStringList getAffectedEntities() const { return m_affectedEntities; }

    // Builder pattern
    ValidationResult& setRuleId(const QString& ruleId) { m_ruleId = ruleId; return *this; }
    ValidationResult& addAffectedEntity(const QString& entityId) { m_affectedEntities.append(entityId); return *this; }

    // Factory methods
    static ValidationResult allowed(const QString& reason = "Operation permitted");
    static ValidationResult blocked(const QString& reason, const QString& ruleId = "");

    // QML integration
    Q_INVOKABLE QVariantMap toVariantMap() const;
};

class InterlockingService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isOperational READ isOperational NOTIFY operationalStateChanged)
    Q_PROPERTY(int activeInterlocks READ getActiveInterlocksCount NOTIFY activeInterlocksChanged)
    Q_PROPERTY(double averageResponseTime READ getAverageResponseTime NOTIFY performanceChanged)

public:
    explicit InterlockingService(DatabaseManager* dbManager, QObject* parent = nullptr);
    ~InterlockingService();

    // Main validation interface
    Q_INVOKABLE ValidationResult validateSignalOperation(const QString& signalId,
                                                         const QString& currentAspect,
                                                         const QString& requestedAspect,
                                                         const QString& operatorId = "HMI_USER");

    Q_INVOKABLE ValidationResult validatePointMachineOperation(const QString& machineId,
                                                               const QString& currentPosition,
                                                               const QString& requestedPosition,
                                                               const QString& operatorId = "HMI_USER");

    Q_INVOKABLE ValidationResult validateTrackAssignment(const QString& trackId,
                                                         bool currentlyAssigned,
                                                         bool requestedAssignment,
                                                         const QString& operatorId = "HMI_USER");

    // System management
    Q_INVOKABLE bool initialize();
    Q_INVOKABLE bool isOperational() const { return m_isOperational; }
    Q_INVOKABLE double getAverageResponseTime() const;
    Q_INVOKABLE int getActiveInterlocksCount() const;
    Q_INVOKABLE void enforceTrackOccupancyInterlocking(const QString& trackId, bool wasOccupied, bool isOccupied);

signals:
    void operationBlocked(const QString& entityId, const QString& reason);
    void automaticProtectionActivated(const QString& entityId, const QString& reason);
    void operationalStateChanged(bool isOperational);
    void activeInterlocksChanged(int count);
    void performanceChanged();
    void criticalSafetyViolation(const QString& entityId, const QString& violation);
    void systemFreezeRequired(const QString& trackId, const QString& reason, const QString& details);

private:
    DatabaseManager* m_dbManager;
    std::unique_ptr<SignalBranch> m_signalBranch;
    std::unique_ptr<TrackCircuitBranch> m_trackBranch;
    std::unique_ptr<PointMachineBranch> m_pointBranch;

    // Performance monitoring
    bool m_isOperational = false;
    mutable std::mutex m_performanceMutex;
    std::deque<double> m_responseTimeHistory;
    static constexpr size_t MAX_RESPONSE_HISTORY = 1000;
    static constexpr int TARGET_RESPONSE_TIME_MS = 50;
};

Q_DECLARE_METATYPE(ValidationResult)
