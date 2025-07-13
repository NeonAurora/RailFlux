#include "SignalBranch.h"
#include "../database/DatabaseManager.h"
#include <QDebug>

SignalBranch::SignalBranch(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent), m_dbManager(dbManager) {}

ValidationResult SignalBranch::validateAspectChange(
    const QString& signalId, const QString& currentAspect,
    const QString& requestedAspect, const QString& operatorId) {

    // 1. Check if signal is active
    auto activeResult = checkSignalActive(signalId);
    if (!activeResult.isAllowed()) return activeResult;

    // 2. Basic transition validation
    auto basicResult = validateBasicTransition(signalId, currentAspect, requestedAspect);
    if (!basicResult.isAllowed()) return basicResult;

    // 3. Track protection validation
    auto trackResult = checkTrackProtection(signalId, requestedAspect);
    if (!trackResult.isAllowed()) return trackResult;

    // 4. Interlocked signals validation
    auto interlockResult = checkInterlockedSignals(signalId, requestedAspect);
    if (!interlockResult.isAllowed()) return interlockResult;

    return ValidationResult::allowed("All signal validations passed");
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

ValidationResult SignalBranch::checkTrackProtection(const QString& signalId, const QString& requestedAspect) {
    // ✅ SAFETY: Only check track protection for proceed aspects
    if (requestedAspect == "RED") {
        return ValidationResult::allowed("RED aspect - no track protection required");
    }

    // ✅ SAFETY: Comprehensive protected tracks validation
    auto validation = validateProtectedTracks(signalId);

    if (!validation.isValid) {
        return ValidationResult::blocked(
            QString("Cannot clear signal %1: %2").arg(signalId, validation.errorReason),
            validation.occupiedTracks.isEmpty() ? "TRACK_PROTECTION_VALIDATION_FAILED" : "TRACK_OCCUPIED"
            );
    }

    // ✅ SUCCESS: All protected tracks are clear
    return ValidationResult::allowed(
        QString("All %1 protected tracks are clear").arg(validation.protectedTracks.size())
        );
}

ValidationResult SignalBranch::checkInterlockedSignals(const QString& signalId, const QString& requestedAspect) {
    QStringList interlockedSignals = getInterlockedSignals(signalId);

    for (const QString& interlockedSignalId : interlockedSignals) {
        auto interlockedData = m_dbManager->getSignalById(interlockedSignalId);
        if (interlockedData.isEmpty()) continue;

        QString interlockedAspect = interlockedData["currentAspect"].toString();

        // Basic opposing signal rule: both signals cannot show proceed aspects
        bool requestedIsProceed = (requestedAspect == "GREEN" || requestedAspect == "YELLOW");
        bool interlockedIsProceed = (interlockedAspect == "GREEN" || interlockedAspect == "YELLOW");

        if (requestedIsProceed && interlockedIsProceed) {
            return ValidationResult::blocked(
                       QString("Cannot set %1 to %2: interlocked signal %3 shows %4")
                           .arg(signalId, requestedAspect, interlockedSignalId, interlockedAspect),
                       "INTERLOCKED_SIGNAL_CONFLICT"
                       ).addAffectedEntity(interlockedSignalId);
        }
    }

    return ValidationResult::allowed();
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

// SignalBranch.cpp - Replace the getProtectedTracks function
QStringList SignalBranch::getProtectedTracks(const QString& signalId) {
    // ✅ SAFETY: Use comprehensive validation for safety-critical track protection
    auto validation = validateProtectedTracks(signalId);

    if (!validation.isValid) {
        qCritical() << "🚨 SAFETY CRITICAL: Protected tracks validation failed for signal"
                    << signalId << ":" << validation.errorReason;

        // ✅ SAFETY: Log to audit system for compliance
        // TODO: Add to audit log with safety_critical = true

        // ✅ SAFETY: Return empty list to force restrictive behavior
        return QStringList();
    }

    return validation.protectedTracks;
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

SignalBranch::ProtectedTracksValidation SignalBranch::validateProtectedTracks(const QString& signalId) {
    ProtectedTracksValidation result;
    result.isValid = false;

    // ✅ SAFETY: Fetch protected tracks from all 3 sources
    QStringList tracksFromSignalData = getProtectedTracksFromSignalData(signalId);
    QStringList tracksFromInterlockingRules = getProtectedTracksFromInterlockingRules(signalId);
    QStringList tracksFromProtectionTable = getProtectedTracksFromProtectionTable(signalId);

    qDebug() << "🔍 SAFETY AUDIT: Protected tracks for signal" << signalId;
    qDebug() << "   From signal data:" << tracksFromSignalData;
    qDebug() << "   From interlocking rules:" << tracksFromInterlockingRules;
    qDebug() << "   From protection table:" << tracksFromProtectionTable;

    // ✅ SAFETY: Check consistency between all sources
    if (!validateTrackConsistency(tracksFromSignalData, tracksFromInterlockingRules,
                                  tracksFromProtectionTable, result)) {
        return result; // Error details already set in validateTrackConsistency
    }

    // ✅ SAFETY: Use protection table as authoritative source (most explicit)
    QStringList authoritative = tracksFromProtectionTable.isEmpty() ?
                                    tracksFromSignalData : tracksFromProtectionTable;

    if (authoritative.isEmpty()) {
        result.errorReason = "No protected tracks found in any source";
        return result;
    }

    // ✅ SAFETY: Check track occupancy status
    if (!validateTrackOccupancy(authoritative, result)) {
        return result; // Error details already set in validateTrackOccupancy
    }

    // ✅ SUCCESS: All validations passed
    result.isValid = true;
    result.protectedTracks = authoritative;

    qDebug() << "✅ SAFETY: Protected tracks validation passed for signal" << signalId
             << "- Tracks:" << result.protectedTracks;

    return result;
}

QStringList SignalBranch::getProtectedTracksFromSignalData(const QString& signalId) {
    auto signalData = m_dbManager->getSignalById(signalId);
    if (signalData.isEmpty()) {
        qWarning() << "⚠️ Signal data not found for:" << signalId;
        return QStringList();
    }

    // ✅ Parse PostgreSQL TEXT[] array from protected_tracks field
    QVariant protectedTracksVar = signalData["protectedTracks"];
    if (!protectedTracksVar.isValid()) {
        return QStringList();
    }

    QString protectedTracksStr = protectedTracksVar.toString();
    if (protectedTracksStr.isEmpty() || protectedTracksStr == "{}") {
        return QStringList();
    }

    // ✅ Parse PostgreSQL array format: {track1,track2,track3}
    protectedTracksStr = protectedTracksStr.mid(1, protectedTracksStr.length() - 2); // Remove { }
    return protectedTracksStr.split(",", Qt::SkipEmptyParts);
}

QStringList SignalBranch::getProtectedTracksFromInterlockingRules(const QString& signalId) {
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

    QStringList tracks;
    if (!query.exec()) {
        qCritical() << "🚨 SAFETY CRITICAL: Failed to query interlocking rules for signal"
                    << signalId << ":" << query.lastError().text();
        return tracks;
    }

    while (query.next()) {
        tracks.append(query.value(0).toString());
    }

    return tracks;
}

QStringList SignalBranch::getProtectedTracksFromProtectionTable(const QString& signalId) {
    QSqlQuery query(m_dbManager->getDatabase());
    query.prepare(R"(
        SELECT protected_track_id
        FROM railway_control.signal_track_protection
        WHERE signal_id = ?
          AND is_active = TRUE
        ORDER BY protected_track_id
    )");
    query.addBindValue(signalId);

    QStringList tracks;
    if (!query.exec()) {
        qCritical() << "🚨 SAFETY CRITICAL: Failed to query signal_track_protection for signal"
                    << signalId << ":" << query.lastError().text();
        return tracks;
    }

    while (query.next()) {
        tracks.append(query.value(0).toString());
    }

    return tracks;
}

bool SignalBranch::validateTrackConsistency(
    const QStringList& fromSignalData,
    const QStringList& fromInterlockingRules,
    const QStringList& fromProtectionTable,
    ProtectedTracksValidation& result) {

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
        result.errorReason = "No protected tracks found in any source";
        return false;
    }

    // ✅ SAFETY: If only one source has data, that's acceptable
    if (nonEmptySources.size() == 1) {
        qDebug() << "ℹ️ Only one source has protected tracks data:" << sourceNames.first();
        return true;
    }

    // ✅ SAFETY: Compare multiple sources for consistency
    QStringList baseline = nonEmptySources.first();
    baseline.sort();

    for (int i = 1; i < nonEmptySources.size(); i++) {
        QStringList comparison = nonEmptySources[i];
        comparison.sort();

        if (baseline != comparison) {
            result.errorReason = QString("Protected tracks mismatch between %1 and %2")
            .arg(sourceNames.first(), sourceNames[i]);
            result.inconsistentSources = sourceNames;

            qCritical() << "🚨 SAFETY CRITICAL: Protected tracks inconsistency detected!";
            qCritical() << "   " << sourceNames.first() << ":" << baseline;
            qCritical() << "   " << sourceNames[i] << ":" << comparison;

            return false;
        }
    }

    qDebug() << "✅ SAFETY: All sources consistent for protected tracks";
    return true;
}

bool SignalBranch::validateTrackOccupancy(
    const QStringList& protectedTracks,
    ProtectedTracksValidation& result) {

    QStringList occupiedTracks;

    for (const QString& trackId : protectedTracks) {
        auto trackData = m_dbManager->getTrackSegmentById(trackId);
        if (trackData.isEmpty()) {
            result.errorReason = QString("Protected track %1 not found in database").arg(trackId);
            qCritical() << "🚨 SAFETY CRITICAL: Protected track not found:" << trackId;
            return false;
        }

        if (trackData["occupied"].toBool()) {
            occupiedTracks.append(trackId);
            QString occupiedBy = trackData["occupiedBy"].toString();

            qWarning() << "⚠️ SAFETY: Protected track" << trackId
                       << "is occupied by" << occupiedBy;
        }
    }

    if (!occupiedTracks.isEmpty()) {
        result.errorReason = QString("Protected tracks are occupied: %1")
        .arg(occupiedTracks.join(", "));
        result.occupiedTracks = occupiedTracks;

        qCritical() << "🚨 SAFETY CRITICAL: Cannot clear signal - protected tracks occupied:"
                    << occupiedTracks;
        return false;
    }

    qDebug() << "✅ SAFETY: All protected tracks are clear";
    return true;
}
