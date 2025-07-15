#pragma once
#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QStringList>
#include "SignalRule.h"
#include "InterlockingService.h"

class DatabaseManager;

class InterlockingRuleEngine : public QObject {
    Q_OBJECT

public:
    explicit InterlockingRuleEngine(DatabaseManager* dbManager, QObject* parent = nullptr);

    bool loadRulesFromResource(const QString& resourcePath = ":/data/signal_interlocking_rules.json");

    // ✅ RENAMED: Clear function name to avoid confusion
    ValidationResult validateInterlockedSignalAspectChange(const QString& signalId,
                                                           const QString& currentAspect,
                                                           const QString& requestedAspect);

    // Information queries
    QStringList getControlledSignals(const QString& signalId) const;
    QStringList getControllingSignals(const QString& signalId) const;
    bool isSignalIndependent(const QString& signalId) const;

private:
    DatabaseManager* m_dbManager;

    // ✅ FIXED: Store objects directly, not unique_ptr
    struct SignalInfo {
        QString signalType;
        bool isIndependent = false;
        QStringList controlledBy;
        QList<SignalRule> rules;  // ✅ CHANGED: Direct objects, not unique_ptr
    };

    QHash<QString, SignalInfo> m_signalRules;  // ✅ FIXED: No unique_ptr

    // Helper methods
    ValidationResult validateControllingSignals(const QString& signalId,
                                                const QString& requestedAspect);
    bool checkConditions(const QList<SignalRule::Condition>& conditions);
    QString getCurrentSignalAspect(const QString& signalId);
    QString getCurrentPointPosition(const QString& pointId);

    // JSON parsing
    bool parseJsonRules(const QJsonObject& rulesObject);
    SignalRule parseRule(const QJsonObject& ruleObject);  // ✅ CHANGED: Return by value
    SignalRule::Condition parseCondition(const QJsonObject& conditionObject);
    SignalRule::AllowedSignal parseAllowedSignal(const QString& signalId, const QJsonArray& aspectsArray);
};
