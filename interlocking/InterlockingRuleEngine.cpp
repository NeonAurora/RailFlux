#include "InterlockingRuleEngine.h"
#include "../database/DatabaseManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

InterlockingRuleEngine::InterlockingRuleEngine(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent), m_dbManager(dbManager) {

    if (!dbManager) {
        qCritical() << "🚨 SAFETY: InterlockingRuleEngine initialized with null DatabaseManager!";
    }
}

bool InterlockingRuleEngine::loadRulesFromResource(const QString& resourcePath) {
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "🚨 SAFETY: Cannot open interlocking rules file:" << resourcePath;
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qCritical() << "🚨 SAFETY: Invalid JSON in interlocking rules:" << parseError.errorString();
        return false;
    }

    QJsonObject rootObject = doc.object();
    QJsonObject rulesObject = rootObject["signal_interlocking_rules"].toObject();

    bool success = parseJsonRules(rulesObject);

    if (success) {
        qDebug() << "✅ Loaded interlocking rules for" << m_signalRules.size() << "signals";
    }

    return success;
}

ValidationResult InterlockingRuleEngine::validateInterlockedSignalAspectChange(
    const QString& signalId, const QString& currentAspect, const QString& requestedAspect) {

    auto signalInfoIt = m_signalRules.find(signalId);
    if (signalInfoIt == m_signalRules.end()) {
        return ValidationResult::blocked(
            QString("Signal %1 not found in interlocking rules").arg(signalId),
            "SIGNAL_NOT_IN_RULES"
            );
    }

    const SignalInfo& signalInfo = signalInfoIt.value();

    if (signalInfo.isIndependent) {
        qDebug() << "✅ Signal" << signalId << "is independent - change allowed";
        return ValidationResult::allowed("Independent signal - no interlocking restrictions");
    }

    return validateControllingSignals(signalId, requestedAspect);
}

ValidationResult InterlockingRuleEngine::validateControllingSignals(
    const QString& signalId, const QString& requestedAspect) {

    auto signalInfoIt = m_signalRules.find(signalId);
    if (signalInfoIt == m_signalRules.end()) {
        qDebug() << "❌ Signal" << signalId << "not found in m_signalRules";
        return ValidationResult::blocked("Signal not found in rules", "SIGNAL_NOT_FOUND");
    }

    const SignalInfo& signalInfo = signalInfoIt.value();

    // Get control mode
    QString controlMode = signalInfo.controlMode.trimmed().toUpper();
    if (controlMode.isEmpty()) {
        controlMode = "AND"; // default
    }
    qDebug() << "🔍 Signal" << signalId << "control_mode:" << controlMode;

    bool anyControllingAllows = false;
    QStringList blockingReasons;

    for (const QString& controllingSignalId : signalInfo.controlledBy) {
        QString controllingAspect = getCurrentSignalAspect(controllingSignalId);
        qDebug() << "🔍 Controlling signal:" << controllingSignalId
                 << "current aspect:" << controllingAspect;

        auto controllingInfoIt = m_signalRules.find(controllingSignalId);
        if (controllingInfoIt == m_signalRules.end()) {
            qDebug() << "⚠️ Controlling signal" << controllingSignalId << "not found in m_signalRules";
            continue;
        }

        const SignalInfo& controllingInfo = controllingInfoIt.value();

        bool aspectAllowed = false;

        for (const SignalRule& rule : controllingInfo.rules) {
            qDebug() << "🔍 Evaluating rule for aspect:" << rule.getWhenAspect();

            if (rule.getWhenAspect() == controllingAspect) {
                qDebug() << "✅ Aspect matches current controlling aspect.";

                qDebug() << "🔍 Checking conditions for rule...";
                if (!checkConditions(rule.getConditions())) {
                    qDebug() << "❌ Conditions failed for controlling signal"
                             << controllingSignalId << "aspect" << controllingAspect;
                    blockingReasons.append(
                        QString("Conditions not met for rule when %1 shows %2")
                            .arg(controllingSignalId, controllingAspect));
                    continue;
                }
                qDebug() << "✅ Conditions passed.";

                qDebug() << "🔍 Checking if rule allows signal" << signalId
                         << "aspect" << requestedAspect;
                if (rule.isSignalAspectAllowed(signalId, requestedAspect)) {
                    qDebug() << "✅ Rule allows requested aspect.";
                    aspectAllowed = true;
                    break; // No need to check more rules for this controlling signal
                } else {
                    qDebug() << "❌ Rule does not allow requested aspect.";
                }
            } else {
                qDebug() << "⚠️ Skipping rule because aspect does not match.";
            }
        }

        if (controlMode == "AND") {
            if (!aspectAllowed) {
                qDebug() << "❌ AND mode: Aspect not allowed by controlling signal"
                         << controllingSignalId << "- blocking immediately.";
                return ValidationResult::blocked(
                           QString("Signal %1 cannot show %2: controlling signal %3 shows %4")
                               .arg(signalId, requestedAspect, controllingSignalId, controllingAspect),
                           "CONTROLLING_SIGNAL_RESTRICTION"
                           ).addAffectedEntity(controllingSignalId);
            }
        }
        else if (controlMode == "OR") {
            if (aspectAllowed) {
                qDebug() << "✅ OR mode: Aspect allowed by controlling signal"
                         << controllingSignalId << "- will allow after checking all.";
                anyControllingAllows = true;
            } else {
                qDebug() << "⚠️ OR mode: Aspect not allowed by controlling signal"
                         << controllingSignalId;
            }
        }
    }

    if (controlMode == "OR") {
        if (anyControllingAllows) {
            qDebug() << "✅ OR mode: At least one controlling signal allows - returning allowed.";
            return ValidationResult::allowed(
                "At least one controlling signal permits the requested aspect");
        }
        qDebug() << "❌ OR mode: No controlling signals allow - blocking.";
        return ValidationResult::blocked(
            QString("Signal %1 cannot show %2: no controlling signals allow it.\nDetails:\n%3")
                .arg(signalId, requestedAspect, blockingReasons.join("\n")),
            "CONTROLLING_SIGNAL_RESTRICTION"
            );
    }

    qDebug() << "✅ AND mode: All controlling signals allowed - returning allowed.";
    return ValidationResult::allowed("All controlling signals permit the requested aspect");
}


bool InterlockingRuleEngine::checkConditions(const QList<SignalRule::Condition>& conditions) {
    for (const SignalRule::Condition& condition : conditions) {
        if (condition.entityType == "point_machine") {
            QString currentPosition = getCurrentPointPosition(condition.entityId);
            if (currentPosition != condition.requiredState) {
                qDebug() << "❌ Condition failed: Point machine" << condition.entityId
                         << "is" << currentPosition << "but requires" << condition.requiredState;
                return false;
            }
        }
        // ✅ FUTURE: Add track_segment and other condition types
        else if (condition.entityType == "track_segment") {
            // Future implementation for trackSegment occupancy conditions
            qDebug() << "ℹ️ Track segment conditions not yet implemented:" << condition.entityId;
        }
    }
    return true;
}

QString InterlockingRuleEngine::getCurrentSignalAspect(const QString& signalId) {
    if (!m_dbManager) {
        qWarning() << "❌ Database manager not available";
        return "RED";
    }

    auto signalData = m_dbManager->getSignalById(signalId);
    return signalData.value("currentAspect", "RED").toString();
}

QString InterlockingRuleEngine::getCurrentPointPosition(const QString& pointId) {
    if (!m_dbManager) {
        qWarning() << "❌ Database manager not available";
        return "NORMAL";
    }

    auto pointData = m_dbManager->getPointMachineById(pointId);
    return pointData.value("position", "NORMAL").toString();
}

bool InterlockingRuleEngine::parseJsonRules(const QJsonObject& rulesObject) {
    m_signalRules.clear();

    for (auto it = rulesObject.begin(); it != rulesObject.end(); ++it) {
        QString signalId = it.key();
        QJsonObject signalObject = it.value().toObject();

        SignalInfo signalInfo;
        signalInfo.signalType = signalObject["type"].toString();
        signalInfo.isIndependent = signalObject["independent"].toBool(false);
        signalInfo.controlMode = signalObject["control_mode"].toString();

        // Parse controlled_by array
        QJsonArray controlledByArray = signalObject["controlled_by"].toArray();
        for (const QJsonValue& value : controlledByArray) {
            signalInfo.controlledBy.append(value.toString());
        }

        // Parse rules array
        QJsonArray rulesArray = signalObject["rules"].toArray();
        for (const QJsonValue& ruleValue : rulesArray) {
            QJsonObject ruleObject = ruleValue.toObject();
            SignalRule rule = parseRule(ruleObject);
            signalInfo.rules.append(rule);
        }

        m_signalRules[signalId] = signalInfo;
    }

    qDebug() << "✅ Parsed" << m_signalRules.size() << "signal rules from JSON";
    return true;
}

SignalRule InterlockingRuleEngine::parseRule(const QJsonObject& ruleObject) {
    QString whenAspect = ruleObject["when_aspect"].toString();

    // Parse conditions
    QList<SignalRule::Condition> conditions;
    QJsonArray conditionsArray = ruleObject["conditions"].toArray();
    for (const QJsonValue& condValue : conditionsArray) {
        QJsonObject condObject = condValue.toObject();
        conditions.append(parseCondition(condObject));
    }

    // Parse allows
    QList<SignalRule::AllowedSignal> allowedSignals;
    QJsonObject allowsObject = ruleObject["allows"].toObject();
    for (auto it = allowsObject.begin(); it != allowsObject.end(); ++it) {
        QString signalId = it.key();
        QJsonArray aspectsArray = it.value().toArray();
        allowedSignals.append(parseAllowedSignal(signalId, aspectsArray));
    }

    return SignalRule(whenAspect, conditions, allowedSignals);
}

// ✅ IMPLEMENTATION: parseCondition
SignalRule::Condition InterlockingRuleEngine::parseCondition(const QJsonObject& conditionObject) {
    SignalRule::Condition condition;

    // Parse point machine condition: {"point_machine": "PM001", "position": "NORMAL"}
    if (conditionObject.contains("point_machine")) {
        condition.entityType = "point_machine";
        condition.entityId = conditionObject["point_machine"].toString();
        condition.requiredState = conditionObject["position"].toString();
    }
    // ✅ FUTURE: Track segment conditions
    else if (conditionObject.contains("track_segment")) {
        condition.entityType = "track_segment";
        condition.entityId = conditionObject["track_segment"].toString();
        condition.requiredState = conditionObject["occupancy"].toString();
    }
    else {
        qWarning() << "⚠️ Unknown condition type in JSON:" << conditionObject;
        condition.entityType = "unknown";
        condition.entityId = "";
        condition.requiredState = "";
    }

    return condition;
}

// ✅ IMPLEMENTATION: parseAllowedSignal
SignalRule::AllowedSignal InterlockingRuleEngine::parseAllowedSignal(const QString& signalId, const QJsonArray& aspectsArray) {
    SignalRule::AllowedSignal allowedSignal;
    allowedSignal.signalId = signalId;

    // Convert JSON array to QStringList
    for (const QJsonValue& aspectValue : aspectsArray) {
        QString aspect = aspectValue.toString();
        if (!aspect.isEmpty()) {
            allowedSignal.allowedAspects.append(aspect);
        }
    }

    return allowedSignal;
}

// ✅ INFORMATION QUERY METHODS (for debugging/inspection)
QStringList InterlockingRuleEngine::getControlledSignals(const QString& signalId) const {
    QStringList controlled;

    auto signalInfoIt = m_signalRules.find(signalId);
    if (signalInfoIt != m_signalRules.end()) {
        const SignalInfo& signalInfo = signalInfoIt.value();

        // Find all signals that list this signal as controlling
        for (const SignalRule& rule : signalInfo.rules) {
            for (const SignalRule::AllowedSignal& allowedSignal : rule.getAllowedSignals()) {
                if (!controlled.contains(allowedSignal.signalId)) {
                    controlled.append(allowedSignal.signalId);
                }
            }
        }
    }

    return controlled;
}

QStringList InterlockingRuleEngine::getControllingSignals(const QString& signalId) const {
    auto signalInfoIt = m_signalRules.find(signalId);
    if (signalInfoIt != m_signalRules.end()) {
        return signalInfoIt.value().controlledBy;
    }
    return QStringList();
}

bool InterlockingRuleEngine::isSignalIndependent(const QString& signalId) const {
    auto signalInfoIt = m_signalRules.find(signalId);
    if (signalInfoIt != m_signalRules.end()) {
        return signalInfoIt.value().isIndependent;
    }
    return false;
}
