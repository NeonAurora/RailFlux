#include "ResourceLockService.h"
#include "../database/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonObject>

namespace RailFlux::Route {

ResourceLockService::ResourceLockService(DatabaseManager* dbManager, QObject* parent)
    : QObject(parent)
    , m_dbManager(dbManager)
    , m_maintenanceTimer(new QTimer(this))
{
    if (!m_dbManager) {
        qCritical() << "ResourceLockService: DatabaseManager is null";
        return;
    }

    // Setup maintenance timer
    m_maintenanceTimer->setInterval(MAINTENANCE_INTERVAL_MS);
    connect(m_maintenanceTimer, &QTimer::timeout, this, &ResourceLockService::performMaintenanceCheck);

    // Connect to database connection changes
    connect(m_dbManager, &DatabaseManager::connectionStateChanged,
            this, [this](bool connected) {
                if (connected) {
                    initialize();
                } else {
                    m_isOperational = false;
                    emit operationalStateChanged();
                }
            });
}

ResourceLockService::~ResourceLockService() {
    if (m_maintenanceTimer) {
        m_maintenanceTimer->stop();
    }
}

void ResourceLockService::initialize() {
    qDebug() << "🔄 ResourceLockService: Initializing...";

    if (!m_dbManager || !m_dbManager->isConnected()) {
        qWarning() << "ResourceLockService: Cannot initialize - database not connected";
        return;
    }

    try {
        // Load existing locks from database
        if (loadLocksFromDatabase()) {
            m_isOperational = true;
            m_maintenanceTimer->start();
            
            qDebug() << "✅ ResourceLockService: Initialized with" << activeLocks() << "active locks";
            emit operationalStateChanged();
            emit lockCountChanged();
        } else {
            qCritical() << "❌ ResourceLockService: Failed to load locks from database";
        }

    } catch (const std::exception& e) {
        qCritical() << "❌ ResourceLockService: Initialization failed:" << e.what();
        m_isOperational = false;
        emit operationalStateChanged();
    }
}

QVariantMap ResourceLockService::lockResource(
    const QString& resourceType,
    const QString& resourceId,
    const QString& routeId,
    const QString& lockType,
    const QString& operatorId,
    const QString& reason,
    int timeoutMinutes
) {
    m_totalLockRequests++;

    if (!m_isOperational) {
        return QVariantMap{
            {"success", false},
            {"error", "ResourceLockService not operational"}
        };
    }

    LockRequest request;
    request.resourceType = resourceType.toUpper();
    request.resourceId = resourceId;
    request.routeId = QUuid::fromString(routeId);
    request.lockType = lockType.toUpper();
    request.operatorId = operatorId;
    request.reason = reason;
    request.timeoutMinutes = qBound(1, timeoutMinutes, MAX_LOCK_DURATION_HOURS * 60);

    // Validate request
    QString validationError;
    if (!validateLockRequest(request, validationError)) {
        return QVariantMap{
            {"success", false},
            {"error", validationError}
        };
    }

    // Attempt to acquire lock
    LockResult result = lockResourceInternal(request);
    
    if (result.success) {
        m_successfulLocks++;
        emit resourceLocked(resourceType, resourceId, routeId);
        emit lockCountChanged();
    }

    return QVariantMap{
        {"success", result.success},
        {"error", result.error},
        {"lockedAt", result.lockedAt},
        {"expiresAt", result.expiresAt},
        {"conflictingLocks", result.conflictingLocks}
    };
}

ResourceLockService::LockResult ResourceLockService::lockResourceInternal(const LockRequest& request) {
    LockResult result;
    QString lockKey = QString("%1:%2").arg(request.resourceType, request.resourceId);

    // Check for conflicts
    QStringList conflicts = findConflictingLocks(request.resourceType, request.resourceId, request.lockType);
    if (!conflicts.isEmpty()) {
        result.success = false;
        result.error = QString("Resource conflicts detected with existing locks");
        result.conflictingLocks = conflicts;
        m_conflictDetections++;
        
        emit lockConflictDetected(request.resourceType, request.resourceId, QVariantMap{
            {"conflictingLocks", conflicts},
            {"requestedLockType", request.lockType},
            {"routeId", request.routeId.toString()}
        });
        
        return result;
    }

    // Create lock
    ResourceLock lock;
    lock.resourceType = request.resourceType;
    lock.resourceId = request.resourceId;
    lock.routeId = request.routeId;
    lock.lockType = request.lockType;
    lock.lockedAt = QDateTime::currentDateTime();
    lock.expiresAt = lock.lockedAt.addSecs(request.timeoutMinutes * 60);
    lock.operatorId = request.operatorId;
    lock.lockReason = request.reason;
    lock.isActive = true;

    // Persist to database
    if (!persistLockToDatabase(lock)) {
        result.success = false;
        result.error = "Failed to persist lock to database";
        return result;
    }

    // Add to memory
    if (!m_activeLocks.contains(lockKey)) {
        m_activeLocks[lockKey] = QList<ResourceLock>();
    }
    m_activeLocks[lockKey].append(lock);

    // Track by route
    if (!m_routeLocks.contains(request.routeId)) {
        m_routeLocks[request.routeId] = QStringList();
    }
    m_routeLocks[request.routeId].append(lockKey);

    result.success = true;
    result.lockedAt = lock.lockedAt;
    result.expiresAt = lock.expiresAt;

    qDebug() << "🔒 ResourceLockService: Locked" << request.resourceType << request.resourceId 
             << "for route" << request.routeId.toString();

    return result;
}

bool ResourceLockService::unlockResource(
    const QString& resourceType,
    const QString& resourceId,
    const QString& routeId
) {
    if (!m_isOperational) {
        return false;
    }

    QUuid uuid = QUuid::fromString(routeId);
    bool success = unlockResourceInternal(resourceType.toUpper(), resourceId, uuid);
    
    if (success) {
        emit resourceUnlocked(resourceType, resourceId, routeId);
        emit lockCountChanged();
    }

    return success;
}

bool ResourceLockService::unlockResourceInternal(
    const QString& resourceType,
    const QString& resourceId,
    const QUuid& routeId
) {
    QString lockKey = QString("%1:%2").arg(resourceType, resourceId);
    
    if (!m_activeLocks.contains(lockKey)) {
        return false; // No locks exist for this resource
    }

    QList<ResourceLock>& locks = m_activeLocks[lockKey];
    bool found = false;
    
    for (int i = locks.size() - 1; i >= 0; --i) {
        if (locks[i].routeId == routeId) {
            ResourceLock lockToRemove = locks[i];
            locks.removeAt(i);
            
            // Remove from database
            removeLockFromDatabase(lockToRemove);
            
            // Remove from route tracking
            if (m_routeLocks.contains(routeId)) {
                m_routeLocks[routeId].removeOne(lockKey);
                if (m_routeLocks[routeId].isEmpty()) {
                    m_routeLocks.remove(routeId);
                }
            }
            
            found = true;
            qDebug() << "🔓 ResourceLockService: Unlocked" << resourceType << resourceId 
                     << "for route" << routeId.toString();
            break;
        }
    }

    // Clean up empty lock lists
    if (locks.isEmpty()) {
        m_activeLocks.remove(lockKey);
    }

    return found;
}

bool ResourceLockService::unlockAllResourcesForRoute(const QString& routeId) {
    if (!m_isOperational) {
        return false;
    }

    QUuid uuid = QUuid::fromString(routeId);
    if (!m_routeLocks.contains(uuid)) {
        return true; // No locks for this route
    }

    QStringList lockKeys = m_routeLocks[uuid];
    bool allSuccess = true;

    for (const QString& lockKey : lockKeys) {
        QStringList parts = lockKey.split(":");
        if (parts.size() == 2) {
            if (!unlockResourceInternal(parts[0], parts[1], uuid)) {
                allSuccess = false;
            }
        }
    }

    if (allSuccess) {
        qDebug() << "🔓 ResourceLockService: Unlocked all resources for route" << routeId;
    }

    return allSuccess;
}

bool ResourceLockService::isResourceLocked(
    const QString& resourceType,
    const QString& resourceId
) const {
    QString lockKey = QString("%1:%2").arg(resourceType.toUpper(), resourceId);
    
    if (!m_activeLocks.contains(lockKey)) {
        return false;
    }

    const QList<ResourceLock>& locks = m_activeLocks[lockKey];
    for (const ResourceLock& lock : locks) {
        if (lock.isActive && !lock.isExpired()) {
            return true;
        }
    }

    return false;
}

QVariantMap ResourceLockService::getResourceLockStatus(
    const QString& resourceType,
    const QString& resourceId
) const {
    QString lockKey = QString("%1:%2").arg(resourceType.toUpper(), resourceId);
    
    QVariantMap status;
    status["isLocked"] = false;
    status["locks"] = QVariantList();

    if (!m_activeLocks.contains(lockKey)) {
        return status;
    }

    const QList<ResourceLock>& locks = m_activeLocks[lockKey];
    QVariantList lockList;
    bool hasActiveLock = false;

    for (const ResourceLock& lock : locks) {
        if (lock.isActive && !lock.isExpired()) {
            hasActiveLock = true;
            lockList.append(lockToVariantMap(lock));
        }
    }

    status["isLocked"] = hasActiveLock;
    status["locks"] = lockList;

    return status;
}

bool ResourceLockService::loadLocksFromDatabase() {
    QSqlQuery query(m_dbManager->database());
    query.prepare(R"(
        SELECT 
            resource_type,
            resource_id,
            route_id,
            lock_type,
            locked_at,
            expires_at,
            operator_id,
            lock_reason,
            is_active
        FROM railway_control.resource_locks
        WHERE is_active = TRUE
        ORDER BY locked_at
    )");

    if (!query.exec()) {
        qCritical() << "ResourceLockService: Failed to load locks from database:" << query.lastError().text();
        return false;
    }

    m_activeLocks.clear();
    m_routeLocks.clear();

    while (query.next()) {
        ResourceLock lock;
        lock.resourceType = query.value("resource_type").toString();
        lock.resourceId = query.value("resource_id").toString();
        lock.routeId = QUuid::fromString(query.value("route_id").toString());
        lock.lockType = query.value("lock_type").toString();
        lock.lockedAt = query.value("locked_at").toDateTime();
        lock.expiresAt = query.value("expires_at").toDateTime();
        lock.operatorId = query.value("operator_id").toString();
        lock.lockReason = query.value("lock_reason").toString();
        lock.isActive = query.value("is_active").toBool();

        // Skip expired locks
        if (lock.isExpired()) {
            continue;
        }

        QString lockKey = lock.lockKey();
        if (!m_activeLocks.contains(lockKey)) {
            m_activeLocks[lockKey] = QList<ResourceLock>();
        }
        m_activeLocks[lockKey].append(lock);

        // Track by route
        if (!m_routeLocks.contains(lock.routeId)) {
            m_routeLocks[lock.routeId] = QStringList();
        }
        m_routeLocks[lock.routeId].append(lockKey);
    }

    qDebug() << "📥 ResourceLockService: Loaded" << activeLocks() << "active locks from database";
    return true;
}

bool ResourceLockService::persistLockToDatabase(const ResourceLock& lock) {
    QSqlQuery query(m_dbManager->database());
    query.prepare(R"(
        INSERT INTO railway_control.resource_locks 
        (resource_type, resource_id, route_id, lock_type, locked_at, expires_at, operator_id, lock_reason, is_active)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(lock.resourceType);
    query.addBindValue(lock.resourceId);
    query.addBindValue(lock.routeId.toString());
    query.addBindValue(lock.lockType);
    query.addBindValue(lock.lockedAt);
    query.addBindValue(lock.expiresAt);
    query.addBindValue(lock.operatorId);
    query.addBindValue(lock.lockReason);
    query.addBindValue(lock.isActive);

    if (!query.exec()) {
        qCritical() << "ResourceLockService: Failed to persist lock to database:" << query.lastError().text();
        return false;
    }

    return true;
}

bool ResourceLockService::removeLockFromDatabase(const ResourceLock& lock) {
    QSqlQuery query(m_dbManager->database());
    query.prepare(R"(
        UPDATE railway_control.resource_locks 
        SET is_active = FALSE
        WHERE resource_type = ? AND resource_id = ? AND route_id = ? AND lock_type = ?
    )");

    query.addBindValue(lock.resourceType);
    query.addBindValue(lock.resourceId);
    query.addBindValue(lock.routeId.toString());
    query.addBindValue(lock.lockType);

    if (!query.exec()) {
        qCritical() << "ResourceLockService: Failed to remove lock from database:" << query.lastError().text();
        return false;
    }

    return true;
}

QStringList ResourceLockService::findConflictingLocks(
    const QString& resourceType,
    const QString& resourceId,
    const QString& lockType
) const {
    QStringList conflicts;
    QString lockKey = QString("%1:%2").arg(resourceType, resourceId);
    
    if (!m_activeLocks.contains(lockKey)) {
        return conflicts; // No existing locks
    }

    const QList<ResourceLock>& existingLocks = m_activeLocks[lockKey];
    
    for (const ResourceLock& existingLock : existingLocks) {
        if (!existingLock.isActive || existingLock.isExpired()) {
            continue;
        }

        // Check compatibility
        LockRequest newRequest;
        newRequest.lockType = lockType;
        
        if (!isLockCompatible(existingLock, newRequest)) {
            conflicts.append(QString("%1 (%2) by route %3")
                           .arg(existingLock.lockType, existingLock.operatorId, existingLock.routeId.toString()));
        }
    }

    return conflicts;
}

bool ResourceLockService::isLockCompatible(const ResourceLock& existingLock, const LockRequest& newRequest) const {
    // EXCLUSIVE locks are never compatible with others
    if (existingLock.lockType == "EXCLUSIVE" || newRequest.lockType == "EXCLUSIVE") {
        return false;
    }

    // SHARED locks are compatible with other SHARED locks
    if (existingLock.lockType == "SHARED" && newRequest.lockType == "SHARED") {
        return true;
    }

    // OVERLAP locks have special rules - for now, treat as exclusive
    return false;
}

bool ResourceLockService::validateLockRequest(const LockRequest& request, QString& error) const {
    if (request.resourceType.isEmpty()) {
        error = "Resource type cannot be empty";
        return false;
    }

    if (request.resourceId.isEmpty()) {
        error = "Resource ID cannot be empty";
        return false;
    }

    if (request.routeId.isNull()) {
        error = "Route ID is invalid";
        return false;
    }

    QStringList validLockTypes = {"EXCLUSIVE", "SHARED", "OVERLAP"};
    if (!validLockTypes.contains(request.lockType)) {
        error = QString("Invalid lock type: %1").arg(request.lockType);
        return false;
    }

    QStringList validResourceTypes = {"TRACK_CIRCUIT", "POINT_MACHINE", "SIGNAL"};
    if (!validResourceTypes.contains(request.resourceType)) {
        error = QString("Invalid resource type: %1").arg(request.resourceType);
        return false;
    }

    return true;
}

void ResourceLockService::performMaintenanceCheck() {
    if (!m_isOperational) {
        return;
    }

    cleanupExpiredLocks();
}

void ResourceLockService::cleanupExpiredLocks() {
    QStringList expiredLockKeys;
    
    for (auto it = m_activeLocks.begin(); it != m_activeLocks.end(); ++it) {
        QList<ResourceLock>& locks = it.value();
        
        for (int i = locks.size() - 1; i >= 0; --i) {
            if (locks[i].isExpired()) {
                ResourceLock expiredLock = locks[i];
                locks.removeAt(i);
                
                // Remove from database
                removeLockFromDatabase(expiredLock);
                
                // Remove from route tracking
                if (m_routeLocks.contains(expiredLock.routeId)) {
                    m_routeLocks[expiredLock.routeId].removeOne(it.key());
                    if (m_routeLocks[expiredLock.routeId].isEmpty()) {
                        m_routeLocks.remove(expiredLock.routeId);
                    }
                }
                
                emit lockExpired(expiredLock.resourceType, expiredLock.resourceId, expiredLock.routeId.toString());
                m_expiredLocksCleanedUp++;
            }
        }
        
        if (locks.isEmpty()) {
            expiredLockKeys.append(it.key());
        }
    }
    
    // Remove empty lock lists
    for (const QString& key : expiredLockKeys) {
        m_activeLocks.remove(key);
    }
    
    if (!expiredLockKeys.isEmpty()) {
        qDebug() << "🧹 ResourceLockService: Cleaned up" << expiredLockKeys.size() << "expired locks";
        emit lockCountChanged();
    }
}

int ResourceLockService::expiredLocks() const {
    // This would require querying the database for expired locks
    // For now, return 0 as we clean them up automatically
    return 0;
}

QVariantMap ResourceLockService::lockToVariantMap(const ResourceLock& lock) const {
    return QVariantMap{
        {"resourceType", lock.resourceType},
        {"resourceId", lock.resourceId},
        {"routeId", lock.routeId.toString()},
        {"lockType", lock.lockType},
        {"lockedAt", lock.lockedAt},
        {"expiresAt", lock.expiresAt},
        {"operatorId", lock.operatorId},
        {"lockReason", lock.lockReason},
        {"isActive", lock.isActive},
        {"isExpired", lock.isExpired()}
    };
}

QVariantMap ResourceLockService::getLockStatistics() const {
    return QVariantMap{
        {"activeLocks", activeLocks()},
        {"totalLockRequests", m_totalLockRequests},
        {"successfulLocks", m_successfulLocks},
        {"conflictDetections", m_conflictDetections},
        {"forceUnlocks", m_forceUnlocks},
        {"expiredLocksCleanedUp", m_expiredLocksCleanedUp},
        {"successRate", m_totalLockRequests > 0 ? (double)m_successfulLocks / m_totalLockRequests * 100.0 : 0.0}
    };
}

// Stub implementations for remaining methods
QVariantMap ResourceLockService::lockMultipleResources(const QVariantList& lockRequests, const QString& routeId, const QString& operatorId) {
    // Implementation would batch multiple lock requests
    Q_UNUSED(lockRequests) Q_UNUSED(routeId) Q_UNUSED(operatorId)
    return QVariantMap{{"success", false}, {"error", "Not implemented"}};
}

QVariantList ResourceLockService::getActiveLocksForRoute(const QString& routeId) const {
    Q_UNUSED(routeId)
    return QVariantList();
}

QVariantList ResourceLockService::getAllActiveLocks() const {
    return QVariantList();
}

QVariantList ResourceLockService::getExpiredLocks() const {
    return QVariantList();
}

} // namespace RailFlux::Route