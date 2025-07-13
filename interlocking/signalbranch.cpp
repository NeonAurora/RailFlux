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

    // Check if transition is valid
    if (!isValidAspectTransition(currentAspect, requestedAspect)) {
        return ValidationResult::blocked(
            QString("Invalid aspect transition from %1 to %2 for signal %3")
                .arg(currentAspect, requestedAspect, signalId),
            "INVALID_TRANSITION"
            );
    }

    // Get signal data to check possible aspects
    auto signalData = m_dbManager->getSignalById(signalId);
    if (signalData.isEmpty()) {
        return ValidationResult::blocked("Signal not found: " + signalId, "SIGNAL_NOT_FOUND");
    }

    QStringList possibleAspects = signalData["possibleAspects"].toStringList();
    if (!possibleAspects.contains(requestedAspect)) {
        return ValidationResult::blocked(
            QString("Aspect %1 not permitted for signal %2").arg(requestedAspect, signalId),
            "ASPECT_NOT_PERMITTED"
            );
    }

    return ValidationResult::allowed();
}

ValidationResult SignalBranch::checkTrackProtection(const QString& signalId, const QString& requestedAspect) {
    // Only check track protection for proceed aspects
    if (requestedAspect == "RED") {
        return ValidationResult::allowed();
    }

    QStringList protectedTracks = getProtectedTracks(signalId);

    for (const QString& trackId : protectedTracks) {
        auto trackData = m_dbManager->getTrackSegmentById(trackId);
        if (trackData.isEmpty()) continue;

        if (trackData["occupied"].toBool()) {
            return ValidationResult::blocked(
                       QString("Cannot clear signal %1: protected track %2 is occupied by %3")
                           .arg(signalId, trackId, trackData["occupiedBy"].toString()),
                       "TRACK_OCCUPIED"
                       ).addAffectedEntity(trackId);
        }
    }

    return ValidationResult::allowed();
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

QStringList SignalBranch::getProtectedTracks(const QString& signalId) {
    // Query database for protected tracks
    // This will be implemented based on your signal_track_protection table
    QStringList tracks;

    // For now, use the signal data from existing schema
    auto signalData = m_dbManager->getSignalById(signalId);
    if (!signalData.isEmpty()) {
        // You'll need to add this relationship to your database
        // tracks = signalData["protectedTracks"].toStringList();
    }

    return tracks;
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
    // Basic railway signaling rules
    if (from == to) return false; // No change needed

    // Any signal can be set to RED (danger)
    if (to == "RED") return true;

    // Can only proceed from RED to YELLOW or GREEN
    if (from == "RED" && (to == "YELLOW" || to == "SINGLE_YELLOW" || to == "DOUBLE_YELLOW" || to == "GREEN")) return true;

    // Can proceed from SINGLE_YELLOW to DOUBLE_YELLOW or DOUBLE_YELLOW to SINGLE_YELLOW
    if (from == "SINGLE_YELLOW" && to == "DOUBLE_YELLOW") return true;
    if (from == "DOUBLE_YELLOW" && to == "SINGLE_YELLOW") return true;

    // Can proceed from YELLOW to GREEN
    if (from == "YELLOW" && to == "GREEN") return true;

    // Direct RED to GREEN is allowed for some signals
    if (from == "RED" && to == "GREEN") return true;

    return false;
}
