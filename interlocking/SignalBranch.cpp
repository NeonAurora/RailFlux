#include "SignalBranch.h"
#include "../database/DatabaseManager.h"
#include "InterlockingRuleEngine.h"
#include <QDebug>

SignalBranch::SignalBranch(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent), m_dbManager(dbManager) {

    if (!dbManager) {
        qCritical() << "🚨 SAFETY: SignalBranch initialized with null DatabaseManager!";
        // Consider throwing or handling this critical error
    }

    // ✅ INITIALIZE RULE ENGINE
    m_ruleEngine = std::make_unique<InterlockingRuleEngine>(dbManager, this);

    if (!m_ruleEngine->loadRulesFromResource()) {
        qCritical() << "🚨 SAFETY: Failed to load interlocking rules - system may not be safe!";
        // Consider setting a safety flag or refusing to operate
    }
}

ValidationResult SignalBranch::validateMainAspectChange(
    const QString& signalId, const QString& currentAspect,
    const QString& requestedAspect, const QString& operatorId) {

    // 1. Check if signal is active
    auto activeResult = checkSignalActive(signalId);
    if (!activeResult.isAllowed()) return activeResult;

    // 2. Basic transition validation
    auto basicResult = validateBasicTransition(signalId, currentAspect, requestedAspect);
    if (!basicResult.isAllowed()) return basicResult;

    // 3. Track Segment protection validation
    auto trackSegmentResult = checkTrackSegmentProtection(signalId, requestedAspect);
    if (!trackSegmentResult.isAllowed()) return trackSegmentResult;

    // ✅ FIXED: Pass currentAspect instead of re-fetching
    auto interlockResult = checkInterlockedSignals(signalId, currentAspect, requestedAspect);
    if (!interlockResult.isAllowed()) return interlockResult;

    return ValidationResult::allowed("All signal validations passed");
}

// ✅ NEW: Add this method to SignalBranch.cpp
ValidationResult SignalBranch::validateSubsidiaryAspectChange(
    const QString& signalId, const QString& aspectType,
    const QString& currentAspect, const QString& requestedAspect,
    const QString& operatorId) {

    qDebug() << "🚦🔧 SIGNAL BRANCH: Subsidiary signal validation:" << signalId
             << "Type:" << aspectType
             << "Transition:" << currentAspect << "→" << requestedAspect;

    // ✅ 1. Check if signal exists and is active
    auto activeResult = checkSignalActive(signalId);
    if (!activeResult.isAllowed()) return activeResult;

    // ✅ 2. Validate aspect type and transition rules
    auto transitionResult = validateSubsidiaryTransition(signalId, aspectType, currentAspect, requestedAspect);
    if (!transitionResult.isAllowed()) return transitionResult;

    // ✅ 3. Check calling-on specific safety rules
    if (aspectType == "CALLING_ON") {
        auto callingOnResult = validateCallingOnSafetyRules(signalId, currentAspect, requestedAspect);
        if (!callingOnResult.isAllowed()) return callingOnResult;
    }

    // ✅ 4. Check loop signal specific rules
    if (aspectType == "LOOP") {
        auto loopResult = validateLoopSignalRules(signalId, currentAspect, requestedAspect);
        if (!loopResult.isAllowed()) return loopResult;
    }

    // ✅ 5. Check interlocking rules (if any apply to subsidiary signals)
    auto interlockResult = checkSubsidiaryInterlocking(signalId, aspectType, currentAspect, requestedAspect);
    if (!interlockResult.isAllowed()) return interlockResult;

    qDebug() << "✅ SIGNAL BRANCH: All subsidiary signal validations passed for" << signalId << aspectType;
    return ValidationResult::allowed("All subsidiary signal validations passed");
}

// ✅ PRIVATE HELPERS: Add these to SignalBranch.cpp
ValidationResult SignalBranch::validateSubsidiaryTransition(
    const QString& signalId, const QString& aspectType,
    const QString& currentAspect, const QString& requestedAspect) {

    qDebug() << "🔧 Validating subsidiary transition:" << aspectType << currentAspect << "→" << requestedAspect;

    // ✅ CALLING-ON: Only OFF ↔ WHITE allowed
    if (aspectType == "CALLING_ON") {
        if (!((currentAspect == "OFF" && requestedAspect == "WHITE") ||
              (currentAspect == "WHITE" && requestedAspect == "OFF"))) {
            return ValidationResult::blocked(
                QString("Invalid calling-on transition: %1 → %2. Only OFF ↔ WHITE allowed.")
                    .arg(currentAspect, requestedAspect),
                "CALLING_ON_INVALID_TRANSITION");
        }
    }
    // ✅ LOOP: Only OFF ↔ YELLOW allowed
    else if (aspectType == "LOOP") {
        if (!((currentAspect == "OFF" && requestedAspect == "YELLOW") ||
              (currentAspect == "YELLOW" && requestedAspect == "OFF"))) {
            return ValidationResult::blocked(
                QString("Invalid loop signal transition: %1 → %2. Only OFF ↔ YELLOW allowed.")
                    .arg(currentAspect, requestedAspect),
                "LOOP_INVALID_TRANSITION");
        }
    }
    // ✅ UNKNOWN TYPE
    else {
        return ValidationResult::blocked(
            QString("Unknown subsidiary aspect type: %1").arg(aspectType),
            "UNKNOWN_SUBSIDIARY_TYPE");
    }

    return ValidationResult::allowed("Valid subsidiary transition");
}

ValidationResult SignalBranch::validateCallingOnSafetyRules(
    const QString& signalId, const QString& currentAspect, const QString& requestedAspect) {

    // ✅ RULE: Calling-on can only be cleared when main signal is at danger
    if (requestedAspect == "WHITE") {
        QString mainAspect = getCurrentMainSignalAspect(signalId);
        if (mainAspect.isEmpty()) {
            return ValidationResult::blocked(
                QString("Cannot determine main signal aspect for %1").arg(signalId),
                "MAIN_ASPECT_UNKNOWN");
        }

        if (mainAspect != "RED") {
            return ValidationResult::blocked(
                QString("Calling-on signal can only be cleared when main signal is at danger. Main signal: %1")
                    .arg(mainAspect),
                "CALLING_ON_MAIN_NOT_DANGER");
        }

        qDebug() << "✅ Calling-on safety check passed: Main signal at danger (" << mainAspect << ")";
    }

    return ValidationResult::allowed("Calling-on safety rules passed");
}

ValidationResult SignalBranch::validateLoopSignalRules(
    const QString& signalId, const QString& currentAspect, const QString& requestedAspect) {

    // ✅ RULE: Loop signal platform/track availability check
    if (requestedAspect == "YELLOW") {
        // TODO: Add platform availability check
        // For now, basic validation - can be enhanced later
        qDebug() << "🔄 Loop signal clearance requested for" << signalId << "- checking platform availability";

        // Future: Check if platform track is clear
        // Future: Check if points are set correctly for loop movement
        // Future: Check if conflicting movements are clear
    }

    return ValidationResult::allowed("Loop signal rules passed");
}

ValidationResult SignalBranch::checkSubsidiaryInterlocking(
    const QString& signalId, const QString& aspectType,
    const QString& currentAspect, const QString& requestedAspect) {

    // ✅ FUTURE: Check if there are any interlocking rules for subsidiary signals
    // For now, most interlocking rules apply to main signals only

    qDebug() << "🔧 Checking subsidiary interlocking for" << signalId << aspectType;

    // Future enhancements:
    // - Check if clearing calling-on affects other signals
    // - Check if loop signal conflicts with main line movements
    // - Validate subsidiary signal combinations

    return ValidationResult::allowed("No subsidiary interlocking violations");
}

QString SignalBranch::getCurrentMainSignalAspect(const QString& signalId) {
    if (!m_dbManager || !m_dbManager->isConnected()) {
        qWarning() << "❌ Cannot get main signal aspect: Database not connected";
        return QString();
    }

    return m_dbManager->getCurrentSignalAspect(signalId);
}

ValidationResult SignalBranch::validateBasicTransition(
    const QString& signalId, const QString& currentAspect, const QString& requestedAspect) {

    // ? Store signal ID for transition validation
    m_currentSignalId = signalId;

    // ? Check if transition is valid
    if (!isValidAspectTransition(currentAspect, requestedAspect)) {
        return ValidationResult::blocked(
            QString("Invalid aspect transition from %1 to %2 for signal %3")
                .arg(currentAspect, requestedAspect, signalId),
            "INVALID_TRANSITION"
        );
    }

    // ? Get signal data to check capabilities
    auto signalData = m_dbManager->getSignalById(signalId);
    if (signalData.isEmpty()) {
        return ValidationResult::blocked("Signal not found: " + signalId, "SIGNAL_NOT_FOUND");
    }

    // ? SAFETY: Validate aspect is supported by this signal type
    QStringList possibleAspects = signalData["possibleAspects"].toStringList();
    if (!possibleAspects.contains(requestedAspect)) {
        return ValidationResult::blocked(
            QString("Aspect %1 not supported by %2 signal %3")
                .arg(requestedAspect, signalData["type"].toString(), signalId),
            "ASPECT_NOT_SUPPORTED"
        );
    }

    return ValidationResult::allowed();
}

ValidationResult SignalBranch::checkTrackSegmentProtection(const QString& signalId, const QString& requestedAspect) {
    // ✅ SAFETY: Only check trackSegment protection for proceed aspects
    if (requestedAspect == "RED") {
        return ValidationResult::allowed("RED aspect - no trackSegment protection required");
    }

    // ✅ SAFETY: Comprehensive protected trackSegments validation
    auto validation = validateProtectedTrackSegments(signalId);

    if (!validation.isValid) {
        return ValidationResult::blocked(
            QString("Cannot clear signal %1: %2").arg(signalId, validation.errorReason),
            validation.occupiedTrackSegments.isEmpty() ? "TRACK_SEGMENT_PROTECTION_VALIDATION_FAILED" : "TRACK_SEGMENT_OCCUPIED"
            );
    }

    // ✅ SUCCESS: All protected trackSegments are clear
    return ValidationResult::allowed(
        QString("All %1 protected trackSegments are clear").arg(validation.protectedTrackSegments.size())
        );
}

ValidationResult SignalBranch::checkInterlockedSignals(
    const QString& signalId,
    const QString& currentAspect,
    const QString& requestedAspect) {

    if (!m_ruleEngine) {
        qCritical() << "🚨 SAFETY: Rule engine not initialized!";
        return ValidationResult::blocked("Interlocking system not available", "RULE_ENGINE_MISSING");
    }

    // ✅ FIXED: Use renamed function
    return m_ruleEngine->validateInterlockedSignalAspectChange(signalId, currentAspect, requestedAspect);
}

ValidationResult SignalBranch::checkSignalActive(const QString& signalId) {
    auto signalData = m_dbManager->getSignalById(signalId);
    if (signalData.isEmpty()) {
        return ValidationResult::blocked("Signal not found: " + signalId, "SIGNAL_NOT_FOUND");
    }

    if (!signalData["isActive"].toBool()) {
        return ValidationResult::blocked("Signal is not active: " + signalId, "SIGNAL_INACTIVE");
    }

    return ValidationResult::allowed();
}

// SignalBranch.cpp - Replace the getProtectedTrackSegments function
QStringList SignalBranch::getProtectedTrackSegments(const QString& signalId) {
    // ✅ SAFETY: Use comprehensive validation for safety-critical trackSegment protection
    auto validation = validateProtectedTrackSegments(signalId);

    if (!validation.isValid) {
        qCritical() << "🚨 SAFETY CRITICAL: Protected trackSegments validation failed for signal"
                    << signalId << ":" << validation.errorReason;

        // ✅ SAFETY: Log to audit system for compliance
        // TODO: Add to audit log with safety_critical = true

        // ✅ SAFETY: Return empty list to force restrictive behavior
        return QStringList();
    }

    return validation.protectedTrackSegments;
}

QStringList SignalBranch::getInterlockedSignals(const QString& signalId) {
    auto signalData = m_dbManager->getSignalById(signalId);
    if (!signalData.isEmpty()) {
        // This should come from your existing interlocked_with field
        return signalData["interlockedWith"].toStringList();
    }
    return QStringList();
}

bool SignalBranch::isValidAspectTransition(const QString& from, const QString& to) {
    // ✅ SAFETY: No change needed if same aspect
    if (from == to) return false;

    // ✅ SAFETY: RED is always accessible for emergency stops
    if (to == "RED") return true;

    // ✅ Get signal capabilities from database to validate transition
    // This prevents invalid capability transitions
    auto signalData = m_dbManager->getSignalById(m_currentSignalId);
    if (signalData.isEmpty()) return false;

    QStringList supportedAspects = signalData["possibleAspects"].toStringList();

    // ✅ SAFETY: Cannot transition to unsupported aspect
    if (!supportedAspects.contains(to)) {
        qDebug() << "🚫 BLOCKED: Signal doesn't support aspect" << to;
        return false;
    }

    // ✅ Check for inter-group transitions (your main concern)
    SignalGroup fromGroup = determineSignalGroup(from);
    SignalGroup toGroup = determineSignalGroup(to);

    if (fromGroup != toGroup) {
        // ✅ SAFETY: Block dangerous inter-group transitions
        if (isDangerousInterGroupTransition(fromGroup, toGroup, from, to)) {
            qDebug() << "🚫 BLOCKED: Dangerous inter-group transition" << from << "→" << to;
            return false;
        }
    }

    // ✅ Allow all other transitions within same group or safe inter-group
    return true;
}

SignalBranch::SignalGroup SignalBranch::determineSignalGroup(const QString& aspect) {
    // ✅ SAFETY: Categorize aspects by their functional groups
    if (aspect == "WHITE") return SignalGroup::CALLING_ON;
    if (aspect == "BLUE") return SignalGroup::SHUNT_SIGNALS;  // Future
    if (aspect == "PURPLE") return SignalGroup::BLOCK_SIGNALS; // Future

    // Main signaling group
    QStringList mainAspects = {"RED", "YELLOW", "GREEN", "SINGLE_YELLOW", "DOUBLE_YELLOW"};
    if (mainAspects.contains(aspect)) return SignalGroup::MAIN_SIGNALS;

    return SignalGroup::MAIN_SIGNALS; // Safe default
}

bool SignalBranch::isDangerousInterGroupTransition(
    SignalGroup fromGroup, SignalGroup toGroup,
    const QString& from, const QString& to) {

    // ✅ SAFETY: Define dangerous transitions

    // WHITE (calling-on) should only transition to/from RED for safety
    if (fromGroup == SignalGroup::CALLING_ON && toGroup == SignalGroup::MAIN_SIGNALS) {
        return to != "RED"; // Only allow WHITE → RED
    }
    if (fromGroup == SignalGroup::MAIN_SIGNALS && toGroup == SignalGroup::CALLING_ON) {
        return from != "RED"; // Only allow RED → WHITE
    }

    // Future: BLUE (shunt) transitions
    if (fromGroup == SignalGroup::SHUNT_SIGNALS || toGroup == SignalGroup::SHUNT_SIGNALS) {
        // Define shunt signal rules when implemented
        return false; // For now, allow (implement rules later)
    }

    // Future: PURPLE (block) transitions
    if (fromGroup == SignalGroup::BLOCK_SIGNALS || toGroup == SignalGroup::BLOCK_SIGNALS) {
        // Define block signal rules when implemented
        return false; // For now, allow (implement rules later)
    }

    return false; // Allow other inter-group transitions
}

SignalBranch::ProtectedTrackSegmentsValidation SignalBranch::validateProtectedTrackSegments(const QString& signalId) {
    ProtectedTrackSegmentsValidation result;
    result.isValid = false;

    // ✅ SAFETY: Fetch protected trackSegments from all 3 sources
    QStringList trackSegmentsFromSignalData = getProtectedTrackSegmentsFromSignalData(signalId);
    QStringList trackSegmentsFromInterlockingRules = getProtectedTrackSegmentsFromInterlockingRules(signalId);
    QStringList trackSegmentsFromProtectionTable = getProtectedTrackSegmentsFromProtectionTable(signalId);

    qDebug() << "🔍 SAFETY AUDIT: Protected trackSegments for signal" << signalId;
    qDebug() << "   From signal data:" << trackSegmentsFromSignalData;
    qDebug() << "   From interlocking rules:" << trackSegmentsFromInterlockingRules;
    qDebug() << "   From protection table:" << trackSegmentsFromProtectionTable;

    // ✅ SAFETY: Check consistency between all sources
    if (!validateTrackSegmentConsistency(trackSegmentsFromSignalData, trackSegmentsFromInterlockingRules,
                                  trackSegmentsFromProtectionTable, result)) {
        return result; // Error details already set in validateTrackSegmentConsistency
    }

    // ✅ SAFETY: Use protection table as authoritative source (most explicit)
    QStringList authoritative = trackSegmentsFromProtectionTable.isEmpty() ?
                                    trackSegmentsFromSignalData : trackSegmentsFromProtectionTable;

    if (authoritative.isEmpty()) {
        result.errorReason = "No protected trackSegments found in any source";
        return result;
    }

    // ✅ SAFETY: Check trackSegment occupancy status
    if (!validateTrackSegmentOccupancy(authoritative, result)) {
        return result; // Error details already set in validateTrackSegmentOccupancy
    }

    // ✅ SUCCESS: All validations passed
    result.isValid = true;
    result.protectedTrackSegments = authoritative;

    qDebug() << "✅ SAFETY: Protected trackSegments validation passed for signal" << signalId
             << "- Track Segments:" << result.protectedTrackSegments;

    return result;
}

QStringList SignalBranch::getProtectedTrackSegmentsFromSignalData(const QString& signalId) {
    auto signalData = m_dbManager->getSignalById(signalId);
    if (signalData.isEmpty()) {
        qWarning() << "⚠️ Signal data not found for:" << signalId;
        return QStringList();
    }

    // ✅ Parse PostgreSQL TEXT[] array from protected_trackSegments field
    QVariant protectedTrackSegmentsVar = signalData["protectedTrackSegments"];
    if (!protectedTrackSegmentsVar.isValid()) {
        return QStringList();
    }

    QString protectedTrackSegmentsStr = protectedTrackSegmentsVar.toString();
    if (protectedTrackSegmentsStr.isEmpty() || protectedTrackSegmentsStr == "{}") {
        return QStringList();
    }

    // ✅ Parse PostgreSQL array format: {trackSegment1,trackSegment2,trackSegment3}
    protectedTrackSegmentsStr = protectedTrackSegmentsStr.mid(1, protectedTrackSegmentsStr.length() - 2); // Remove { }
    return protectedTrackSegmentsStr.split(",", Qt::SkipEmptyParts);
}

QStringList SignalBranch::getProtectedTrackSegmentsFromInterlockingRules(const QString& signalId) {
    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        SELECT target_entity_id
        FROM railway_control.interlocking_rules
        WHERE source_entity_type = 'SIGNAL'
          AND source_entity_id = ?
          AND target_entity_type = 'TRACK_SEGMENT'
          AND target_constraint IN ('MUST_BE_CLEAR', 'PROTECTING')
          AND is_active = TRUE
        ORDER BY target_entity_id
    )");
    query.addBindValue(signalId);

    QStringList trackSegments;
    if (!query.exec()) {
        qCritical() << "🚨 SAFETY CRITICAL: Failed to query interlocking rules for signal"
                    << signalId << ":" << query.lastError().text();
        return trackSegments;
    }

    while (query.next()) {
        trackSegments.append(query.value(0).toString());
    }

    return trackSegments;
}

QStringList SignalBranch::getProtectedTrackSegmentsFromProtectionTable(const QString& signalId) {
    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        SELECT protected_track_segment_id
        FROM railway_control.signal_track_segment_protection
        WHERE signal_id = ?
          AND is_active = TRUE
        ORDER BY protected_track_segment_id
    )");
    query.addBindValue(signalId);

    QStringList trackSegments;
    if (!query.exec()) {
        qCritical() << "🚨 SAFETY CRITICAL: Failed to query signal_track_segment_protection for signal"
                    << signalId << ":" << query.lastError().text();
        return trackSegments;
    }

    while (query.next()) {
        trackSegments.append(query.value(0).toString());
    }

    return trackSegments;
}

bool SignalBranch::validateTrackSegmentConsistency(
    const QStringList& fromSignalData,
    const QStringList& fromInterlockingRules,
    const QStringList& fromProtectionTable,
    ProtectedTrackSegmentsValidation& result) {

    // ✅ SAFETY: Compare all non-empty sources for consistency
    QList<QStringList> nonEmptySources;
    QStringList sourceNames;

    if (!fromSignalData.isEmpty()) {
        nonEmptySources.append(fromSignalData);
        sourceNames.append("signal_data");
    }
    if (!fromInterlockingRules.isEmpty()) {
        nonEmptySources.append(fromInterlockingRules);
        sourceNames.append("interlocking_rules");
    }
    if (!fromProtectionTable.isEmpty()) {
        nonEmptySources.append(fromProtectionTable);
        sourceNames.append("protection_table");
    }

    if (nonEmptySources.isEmpty()) {
        result.errorReason = "No protected trackSegments found in any source";
        return false;
    }

    // ✅ SAFETY: If only one source has data, that's acceptable
    if (nonEmptySources.size() == 1) {
        qDebug() << "ℹ️ Only one source has protected trackSegments data:" << sourceNames.first();
        return true;
    }

    // ✅ SAFETY: Compare multiple sources for consistency
    QStringList baseline = nonEmptySources.first();
    baseline.sort();

    for (int i = 1; i < nonEmptySources.size(); i++) {
        QStringList comparison = nonEmptySources[i];
        comparison.sort();

        if (baseline != comparison) {
            result.errorReason = QString("Protected trackSegments mismatch between %1 and %2")
            .arg(sourceNames.first(), sourceNames[i]);
            result.inconsistentSources = sourceNames;

            qCritical() << "🚨 SAFETY CRITICAL: Protected trackSegments inconsistency detected!";
            qCritical() << "   " << sourceNames.first() << ":" << baseline;
            qCritical() << "   " << sourceNames[i] << ":" << comparison;

            return false;
        }
    }

    qDebug() << "✅ SAFETY: All sources consistent for protected trackSegments";
    return true;
}

bool SignalBranch::validateTrackSegmentOccupancy(
    const QStringList& protectedTrackSegments,
    ProtectedTrackSegmentsValidation& result) {

    QStringList occupiedTrackSegments;

    for (const QString& trackSegmentId : protectedTrackSegments) {
        auto trackSegmentData = m_dbManager->getTrackSegmentById(trackSegmentId);
        if (trackSegmentData.isEmpty()) {
            result.errorReason = QString("Protected track segment %1 not found in database").arg(trackSegmentId);
            qCritical() << "🚨 SAFETY CRITICAL: Protected track segment not found:" << trackSegmentId;
            return false;
        }

        if (trackSegmentData["occupied"].toBool()) {
            occupiedTrackSegments.append(trackSegmentId);
            QString occupiedBy = trackSegmentData["occupiedBy"].toString();

            qWarning() << "⚠️ SAFETY: Protected track segment" << trackSegmentId
                       << "is occupied by" << occupiedBy;
        }
    }

    if (!occupiedTrackSegments.isEmpty()) {
        result.errorReason = QString("Protected trackSegments are occupied: %1")
        .arg(occupiedTrackSegments.join(", "));
        result.occupiedTrackSegments = occupiedTrackSegments;

        qCritical() << "🚨 SAFETY CRITICAL: Cannot clear signal - protected trackSegments occupied:"
                    << occupiedTrackSegments;
        return false;
    }

    qDebug() << "✅ SAFETY: All protected trackSegments are clear";
    return true;
}
