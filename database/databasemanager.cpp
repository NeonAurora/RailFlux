#include "DatabaseManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QThread>

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
    , pollingTimer(std::make_unique<QTimer>(this))
    , connected(false)
{
    connect(pollingTimer.get(), &QTimer::timeout, this, &DatabaseManager::pollDatabase);
    pollingTimer->setInterval(POLLING_INTERVAL_MS);
}

DatabaseManager::~DatabaseManager() {
    stopPolling();
    if (db.isOpen()) {
        db.close();
    }
}

bool DatabaseManager::connectToDatabase() {
    if (db.isOpen()) {
        db.close();
    }

    // Create a unique connection for the manager
    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName("localhost");
    db.setDatabaseName("railway_control_system");
    db.setUserName("postgres");
    db.setPassword("qwerty"); // TODO: Move to config
    db.setPort(5432);

    if (!db.open()) {
        logError("Database connection", db.lastError());
        connected = false;
    } else {
        connected = true;
        qDebug() << "✅ SAFETY: Database connected successfully - NO CACHING";
        setupDatabase();
        enableRealTimeUpdates();
    }

    emit connectionStateChanged(connected);
    return connected;
}

void DatabaseManager::enableRealTimeUpdates() {
    if (!connected) {
        qDebug() << "Cannot enable real-time updates - database not connected";
        return;
    }

    // Try to enable PostgreSQL notifications
    QSqlQuery query(db);
    if (query.exec("LISTEN railway_changes")) {
        qDebug() << "PostgreSQL LISTEN enabled for real-time updates";

        QObject::connect(db.driver(), &QSqlDriver::notification,
                         this, [this](const QString& name, QSqlDriver::NotificationSource /*source*/, const QVariant& payload) {
                             this->handleDatabaseNotification(name, payload);
                         });
    } else {
        qWarning() << "Failed to enable PostgreSQL LISTEN - using polling only";
        qWarning() << "Error:" << query.lastError().text();
    }
}

void DatabaseManager::handleDatabaseNotification(const QString& name, const QVariant& payload) {
    if (name == "railway_changes") {
        QJsonDocument doc = QJsonDocument::fromJson(payload.toString().toUtf8());
        QJsonObject obj = doc.object();

        QString table = obj["table"].toString();
        QString operation = obj["operation"].toString();
        QString entityId = obj["entity_id"].toString();

        qDebug() << "🔔 REAL-TIME notification:" << table << operation << entityId;

        // ✅ SAFETY: No cache refreshing - just emit signals for UI updates
        if (table == "signals") {
            emit signalsChanged();
            emit signalUpdated(entityId);
        } else if (table == "point_machines") {
            emit pointMachinesChanged();
            emit pointMachineUpdated(entityId);
        } else if (table == "track_segments") {
            emit trackSegmentsChanged();
            emit trackSegmentUpdated(entityId);
        }

        emit dataUpdated();
    }
}

void DatabaseManager::startPolling() {
    if (connected) {
        pollingTimer->start();
        qDebug() << "🔍 SAFETY: Database polling started (interval:" << POLLING_INTERVAL_MS << "ms) - DIRECT QUERIES ONLY";
    }
}

void DatabaseManager::stopPolling() {
    pollingTimer->stop();
    qDebug() << "Database polling stopped";
}

bool DatabaseManager::isConnected() const {
    return connected;
}

void DatabaseManager::pollDatabase() {
    if (!connected) return;

    qDebug() << "🔍 SAFETY POLLING: Direct database state check";
    detectAndEmitChanges();
    emit dataUpdated(); // Trigger QML property updates
}

void DatabaseManager::detectAndEmitChanges() {
    // Poll signals
    QSqlQuery query("SELECT signal_id, current_aspect_id FROM railway_control.signals", db);
    while (query.next()) {
        QString signalId = query.value(0).toString();
        int aspectId = query.value(1).toInt();

        if (!lastSignalStates.contains(signalId.toInt()) || lastSignalStates[signalId.toInt()] != QString::number(aspectId)) {
            lastSignalStates[signalId.toInt()] = QString::number(aspectId);
            emit signalStateChanged(signalId.toInt(), QString::number(aspectId));
        }
    }

    // Poll track circuits
    QSqlQuery trackQuery("SELECT segment_id, is_occupied FROM railway_control.track_segments", db);
    while (trackQuery.next()) {
        QString segmentId = trackQuery.value(0).toString();
        bool isOccupied = trackQuery.value(1).toBool();

        if (!lastTrackStates.contains(segmentId.toInt()) || lastTrackStates[segmentId.toInt()] != isOccupied) {
            lastTrackStates[segmentId.toInt()] = isOccupied;
            emit trackCircuitStateChanged(segmentId.toInt(), isOccupied);
        }
    }
}

// ✅ SAFETY: Direct database queries - NO CACHING
QVariantList DatabaseManager::getTrackSegmentsList() {
    if (!connected) return QVariantList();

    qDebug() << "🔍 SAFETY: getTrackSegmentsList() - DIRECT DATABASE QUERY";

    QVariantList tracks;
    QSqlQuery trackQuery(db);
    QString trackSql = "SELECT segment_id, segment_name, start_row, start_col, end_row, end_col, track_type, is_occupied, is_assigned, occupied_by, is_active FROM railway_control.track_segments ORDER BY segment_id";

    if (trackQuery.exec(trackSql)) {
        while (trackQuery.next()) {
            tracks.append(convertTrackRowToVariant(trackQuery));
        }
    } else {
        qWarning() << "❌ SAFETY CRITICAL: Track query failed:" << trackQuery.lastError().text();
    }

    return tracks;
}

QVariantList DatabaseManager::getAllSignalsList() {
    if (!connected) return QVariantList();

    qDebug() << "🔍 SAFETY: getAllSignalsList() - DIRECT DATABASE QUERY";

    // ✅ FIXED: Changed from 'signals' to 'signalsList' (signals is a Qt keyword)
    QVariantList signalsList;

    QSqlQuery signalQuery(db);
    QString signalSql = R"(
        SELECT s.signal_id, s.signal_name, st.type_code as signal_type,
               s.location_row as row, s.location_col as col, s.direction,
               sa.aspect_code as current_aspect, s.calling_on_aspect, s.loop_aspect,
               s.loop_signal_configuration, s.aspect_count, s.possible_aspects,
               s.is_active, s.location_description as location
        FROM railway_control.signals s
        JOIN railway_config.signal_types st ON s.signal_type_id = st.id
        LEFT JOIN railway_config.signal_aspects sa ON s.current_aspect_id = sa.id
        ORDER BY s.signal_id
    )";

    if (signalQuery.exec(signalSql)) {
        qDebug() << "📊 SAFETY: REAL-TIME signal query executed successfully";
        while (signalQuery.next()) {
            QString signalId = signalQuery.value("signal_id").toString();
            QString currentAspect = signalQuery.value("current_aspect").toString();

            // ✅ SAFETY: Log every signal state from database
            qDebug() << "🚦 REAL-TIME:" << signalId << "=" << currentAspect;

            // ✅ FIXED: Use signalsList instead of signals
            signalsList.append(convertSignalRowToVariant(signalQuery));
        }
    } else {
        qWarning() << "❌ SAFETY CRITICAL: Signal query failed:" << signalQuery.lastError().text();
    }

    // ✅ FIXED: Use signalsList instead of signals
    qDebug() << "✅ SAFETY: Returning" << signalsList.size() << "signals from DIRECT database query";
    return signalsList;
}

QVariantList DatabaseManager::getAllPointMachinesList() {
    if (!connected) return QVariantList();

    qDebug() << "🔍 SAFETY: getAllPointMachinesList() - DIRECT DATABASE QUERY";

    QVariantList points;
    QSqlQuery pointQuery(db);
    QString pointSql = R"(
        SELECT pm.machine_id, pm.machine_name, pm.junction_row, pm.junction_col,
               pm.root_track_connection, pm.normal_track_connection, pm.reverse_track_connection,
               pp.position_code as position, pm.operating_status, pm.transition_time_ms
        FROM railway_control.point_machines pm
        LEFT JOIN railway_config.point_positions pp ON pm.current_position_id = pp.id
        ORDER BY pm.machine_id
    )";

    if (pointQuery.exec(pointSql)) {
        while (pointQuery.next()) {
            points.append(convertPointMachineRowToVariant(pointQuery));
        }
    } else {
        qWarning() << "❌ SAFETY CRITICAL: Point machine query failed:" << pointQuery.lastError().text();
    }

    return points;
}

QVariantList DatabaseManager::getTextLabelsList() {
    if (!connected) return QVariantList();

    qDebug() << "🔍 SAFETY: getTextLabelsList() - DIRECT DATABASE QUERY";

    QVariantList labels;
    QSqlQuery labelQuery(db);
    QString labelSql = "SELECT label_text, position_row, position_col, font_size, color, font_family, is_visible, label_type FROM railway_control.text_labels ORDER BY id";

    if (labelQuery.exec(labelSql)) {
        while (labelQuery.next()) {
            QVariantMap label;
            label["text"] = labelQuery.value("label_text").toString();
            label["row"] = labelQuery.value("position_row").toDouble();
            label["col"] = labelQuery.value("position_col").toDouble();
            label["fontSize"] = labelQuery.value("font_size").toInt();
            label["color"] = labelQuery.value("color").toString();
            label["fontFamily"] = labelQuery.value("font_family").toString();
            label["isVisible"] = labelQuery.value("is_visible").toBool();
            label["type"] = labelQuery.value("label_type").toString();
            labels.append(label);
        }
    } else {
        qWarning() << "❌ SAFETY CRITICAL: Text label query failed:" << labelQuery.lastError().text();
    }

    return labels;
}

QVariantList DatabaseManager::getOuterSignalsList() {
    QVariantList result;
    QVariantList allSignals = getAllSignalsList();  // This is fine

    for (const auto& signalVar : allSignals) {
        QVariantMap signal = signalVar.toMap();
        if (signal["type"].toString() == "OUTER") {
            result.append(signal);
        }
    }

    return result;
}

QVariantList DatabaseManager::getHomeSignalsList() {
    QVariantList result;
    QVariantList allSignals = getAllSignalsList();  // This is fine

    for (const auto& signalVar : allSignals) {
        QVariantMap signal = signalVar.toMap();
        if (signal["type"].toString() == "HOME") {
            result.append(signal);
        }
    }

    return result;
}

QVariantList DatabaseManager::getStarterSignalsList() {
    QVariantList result;
    QVariantList allSignals = getAllSignalsList();  // This is fine

    for (const auto& signalVar : allSignals) {
        QVariantMap signal = signalVar.toMap();
        if (signal["type"].toString() == "STARTER") {
            result.append(signal);
        }
    }

    return result;
}

QVariantList DatabaseManager::getAdvanceStarterSignalsList() {
    QVariantList result;
    QVariantList allSignals = getAllSignalsList();  // This is fine

    for (const auto& signalVar : allSignals) {
        QVariantMap signal = signalVar.toMap();
        if (signal["type"].toString() == "ADVANCED_STARTER") {
            result.append(signal);
        }
    }

    return result;
}

// ✅ SAFETY: Individual object queries - DIRECT DATABASE
QVariantMap DatabaseManager::getSignalById(const QString& signalId) {
    if (!connected) return QVariantMap();

    qDebug() << "🔍 SAFETY: getSignalById(" << signalId << ") - DIRECT DATABASE QUERY";

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT s.signal_id, s.signal_name, st.type_code as signal_type,
               s.location_row as row, s.location_col as col, s.direction,
               sa.aspect_code as current_aspect, s.calling_on_aspect, s.loop_aspect,
               s.loop_signal_configuration, s.aspect_count, s.possible_aspects,
               s.is_active, s.location_description as location
        FROM railway_control.signals s
        JOIN railway_config.signal_types st ON s.signal_type_id = st.id
        LEFT JOIN railway_config.signal_aspects sa ON s.current_aspect_id = sa.id
        WHERE s.signal_id = ?
    )");
    query.addBindValue(signalId);

    if (query.exec() && query.next()) {
        return convertSignalRowToVariant(query);
    }

    qWarning() << "❌ SAFETY: Signal" << signalId << "not found in database";
    return QVariantMap();
}

QVariantMap DatabaseManager::getTrackSegmentById(const QString& segmentId) {
    if (!connected) return QVariantMap();

    qDebug() << "🔍 SAFETY: getTrackSegmentById(" << segmentId << ") - DIRECT DATABASE QUERY";

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT segment_id, segment_name, start_row, start_col, end_row, end_col,
               track_type, is_occupied, is_assigned, occupied_by, is_active
        FROM railway_control.track_segments
        WHERE segment_id = ?
    )");
    query.addBindValue(segmentId);

    if (query.exec() && query.next()) {
        return convertTrackRowToVariant(query);
    }

    qWarning() << "❌ SAFETY: Track segment" << segmentId << "not found in database";
    return QVariantMap();
}

QVariantMap DatabaseManager::getPointMachineById(const QString& machineId) {
    if (!connected) return QVariantMap();

    qDebug() << "🔍 SAFETY: getPointMachineById(" << machineId << ") - DIRECT DATABASE QUERY";

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT pm.machine_id, pm.machine_name, pm.junction_row, pm.junction_col,
               pm.root_track_connection, pm.normal_track_connection, pm.reverse_track_connection,
               pp.position_code as position, pm.operating_status, pm.transition_time_ms
        FROM railway_control.point_machines pm
        LEFT JOIN railway_config.point_positions pp ON pm.current_position_id = pp.id
        WHERE pm.machine_id = ?
    )");
    query.addBindValue(machineId);

    if (query.exec() && query.next()) {
        return convertPointMachineRowToVariant(query);
    }

    qWarning() << "❌ SAFETY: Point machine" << machineId << "not found in database";
    return QVariantMap();
}

// ✅ SAFETY: Update operations - NO CACHE INVALIDATION
bool DatabaseManager::updateSignalAspect(const QString& signalId, const QString& newAspect) {
    if (!connected) return false;

    qDebug() << "🔄 SAFETY: Updating signal:" << signalId << "to aspect:" << newAspect;

    QSqlQuery query(db);
    query.prepare("SELECT railway_control.update_signal_aspect(?, ?, 'HMI_USER')");
    query.addBindValue(signalId);
    query.addBindValue(newAspect);

    if (query.exec() && query.next()) {
        bool success = query.value(0).toBool();
        qDebug() << "✅ SAFETY: Database function returned:" << success;

        if (success) {
            // ✅ SAFETY: Verify the change actually happened
            QSqlQuery verifyQuery(db);
            verifyQuery.prepare("SELECT current_aspect_id FROM railway_control.signals WHERE signal_id = ?");
            verifyQuery.addBindValue(signalId);
            if (verifyQuery.exec() && verifyQuery.next()) {
                int currentAspectId = verifyQuery.value(0).toInt();
                qDebug() << "🔍 SAFETY: Signal" << signalId << "now has aspect_id:" << currentAspectId;
            }

            // ✅ SAFETY: No cache invalidation - just emit signals
            emit signalUpdated(signalId);
            emit signalsChanged();
        }
        return success;
    } else {
        qDebug() << "❌ SAFETY CRITICAL: Database query failed:" << query.lastError().text();
    }

    return false;
}

bool DatabaseManager::updatePointMachinePosition(const QString& machineId, const QString& newPosition) {
    if (!connected) return false;

    qDebug() << "🔄 SAFETY: Updating point machine:" << machineId << "to position:" << newPosition;

    QSqlQuery query(db);
    query.prepare("SELECT railway_control.update_point_position(?, ?, 'HMI_USER')");
    query.addBindValue(machineId);
    query.addBindValue(newPosition);

    if (query.exec() && query.next()) {
        bool success = query.value(0).toBool();
        if (success) {
            // ✅ SAFETY: No cache invalidation - just emit signals
            emit pointMachineUpdated(machineId);
            emit pointMachinesChanged();
        }
        return success;
    }

    qWarning() << "❌ SAFETY CRITICAL: Point machine update failed:" << query.lastError().text();
    return false;
}

bool DatabaseManager::updateTrackOccupancy(const QString& segmentId, bool isOccupied) {
    if (!connected) return false;

    qDebug() << "🔄 SAFETY: Updating track occupancy:" << segmentId << "to" << isOccupied;

    QSqlQuery query(db);
    query.prepare("SELECT railway_control.update_track_occupancy(?, ?, NULL, 'HMI_USER')");
    query.addBindValue(segmentId);
    query.addBindValue(isOccupied);

    if (query.exec() && query.next()) {
        bool success = query.value(0).toBool();
        if (success) {
            // ✅ SAFETY: No cache invalidation - just emit signals
            emit trackSegmentUpdated(segmentId);
            emit trackSegmentsChanged();
        }
        return success;
    }

    qWarning() << "❌ SAFETY CRITICAL: Track occupancy update failed:" << query.lastError().text();
    return false;
}

bool DatabaseManager::updateTrackAssignment(const QString& segmentId, bool isAssigned) {
    if (!connected) return false;

    qDebug() << "🔄 SAFETY: Updating track assignment:" << segmentId << "to" << isAssigned;

    QSqlQuery query(db);
    query.prepare("SELECT railway_control.update_track_assignment(?, ?, 'HMI_USER')");
    query.addBindValue(segmentId);
    query.addBindValue(isAssigned);

    if (query.exec() && query.next()) {
        bool success = query.value(0).toBool();
        if (success) {
            // ✅ SAFETY: No cache invalidation - just emit signals
            emit trackSegmentUpdated(segmentId);
            emit trackSegmentsChanged();
        }
        return success;
    }

    qWarning() << "❌ SAFETY CRITICAL: Track assignment update failed:" << query.lastError().text();
    return false;
}

// ✅ SAFETY: Row conversion helpers (unchanged)
QVariantMap DatabaseManager::convertSignalRowToVariant(const QSqlQuery& query) {
    QVariantMap signal;
    signal["id"] = query.value("signal_id").toString();
    signal["name"] = query.value("signal_name").toString();
    signal["type"] = query.value("signal_type").toString();
    signal["row"] = query.value("row").toDouble();
    signal["col"] = query.value("col").toDouble();
    signal["direction"] = query.value("direction").toString();
    signal["currentAspect"] = query.value("current_aspect").toString();
    signal["callingOnAspect"] = query.value("calling_on_aspect").toString();
    signal["loopAspect"] = query.value("loop_aspect").toString();
    signal["loopSignalConfiguration"] = query.value("loop_signal_configuration").toString();
    signal["aspectCount"] = query.value("aspect_count").toInt();
    signal["isActive"] = query.value("is_active").toBool();
    signal["location"] = query.value("location").toString();

    // Convert PostgreSQL array to QStringList
    QString aspectsStr = query.value("possible_aspects").toString();
    if (!aspectsStr.isEmpty()) {
        aspectsStr = aspectsStr.mid(1, aspectsStr.length() - 2); // Remove { }
        signal["possibleAspects"] = aspectsStr.split(",");
    } else {
        signal["possibleAspects"] = QStringList();
    }

    return signal;
}

QVariantMap DatabaseManager::convertTrackRowToVariant(const QSqlQuery& query) {
    QVariantMap track;
    track["id"] = query.value("segment_id").toString();
    track["name"] = query.value("segment_name").toString();
    track["startRow"] = query.value("start_row").toDouble();
    track["startCol"] = query.value("start_col").toDouble();
    track["endRow"] = query.value("end_row").toDouble();
    track["endCol"] = query.value("end_col").toDouble();
    track["trackType"] = query.value("track_type").toString();
    track["occupied"] = query.value("is_occupied").toBool();
    track["assigned"] = query.value("is_assigned").toBool();
    track["occupiedBy"] = query.value("occupied_by").toString();
    track["isActive"] = query.value("is_active").toBool();

    return track;
}

QVariantMap DatabaseManager::convertPointMachineRowToVariant(const QSqlQuery& query) {
    QVariantMap pm;
    pm["id"] = query.value("machine_id").toString();
    pm["name"] = query.value("machine_name").toString();
    pm["position"] = query.value("position").toString();
    pm["operatingStatus"] = query.value("operating_status").toString();
    pm["transitionTime"] = query.value("transition_time_ms").toInt();

    // Junction point
    QVariantMap junctionPoint;
    junctionPoint["row"] = query.value("junction_row").toDouble();
    junctionPoint["col"] = query.value("junction_col").toDouble();
    pm["junctionPoint"] = junctionPoint;

    // Track connections (parse JSON)
    QString rootConnStr = query.value("root_track_connection").toString();
    QString normalConnStr = query.value("normal_track_connection").toString();
    QString reverseConnStr = query.value("reverse_track_connection").toString();

    if (!rootConnStr.isEmpty()) {
        QJsonDocument rootDoc = QJsonDocument::fromJson(rootConnStr.toUtf8());
        pm["rootTrack"] = rootDoc.object().toVariantMap();
    }

    if (!normalConnStr.isEmpty()) {
        QJsonDocument normalDoc = QJsonDocument::fromJson(normalConnStr.toUtf8());
        pm["normalTrack"] = normalDoc.object().toVariantMap();
    }

    if (!reverseConnStr.isEmpty()) {
        QJsonDocument reverseDoc = QJsonDocument::fromJson(reverseConnStr.toUtf8());
        pm["reverseTrack"] = reverseDoc.object().toVariantMap();
    }

    return pm;
}

// Legacy methods for compatibility
QVariantMap DatabaseManager::getAllSignalStates() {
    QVariantMap states;
    QSqlQuery query("SELECT signal_id, current_aspect_id FROM railway_control.signals", db);
    while (query.next()) {
        states[query.value(0).toString()] = query.value(1).toString();
    }
    return states;
}

QString DatabaseManager::getSignalState(int signalId) {
    QSqlQuery query(db);
    query.prepare("SELECT current_aspect_id FROM railway_control.signals WHERE signal_id = ?");
    query.addBindValue(QString::number(signalId));
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "RED"; // Safe default
}

bool DatabaseManager::getTrackOccupancy(int circuitId) {
    QSqlQuery query(db);
    query.prepare("SELECT is_occupied FROM railway_control.track_segments WHERE segment_id = ?");
    query.addBindValue(QString::number(circuitId));
    if (query.exec() && query.next()) {
        return query.value(0).toBool();
    }
    return false; // Safe default
}

QVariantMap DatabaseManager::getAllTrackCircuitStates() {
    QVariantMap states;
    QSqlQuery query("SELECT segment_id, is_occupied FROM railway_control.track_segments", db);
    while (query.next()) {
        states[query.value(0).toString()] = query.value(1).toBool();
    }
    return states;
}

QVariantMap DatabaseManager::getAllPointMachineStates() {
    QVariantMap states;
    QSqlQuery query("SELECT machine_id, current_position_id FROM railway_control.point_machines", db);
    while (query.next()) {
        states[query.value(0).toString()] = query.value(1).toString();
    }
    return states;
}

QString DatabaseManager::getPointPosition(int machineId) {
    QSqlQuery query(db);
    query.prepare("SELECT current_position_id FROM railway_control.point_machines WHERE machine_id = ?");
    query.addBindValue(QString::number(machineId));
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "NORMAL"; // Safe default
}

bool DatabaseManager::setupDatabase() {
    // Basic setup - tables should already exist from DatabaseInitializer
    return true;
}

void DatabaseManager::logError(const QString& operation, const QSqlError& error) {
    qWarning() << "Database error in" << operation << ":" << error.text();
}
