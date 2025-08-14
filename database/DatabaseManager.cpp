#include "DatabaseManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QThread>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QSqlRecord>
#include "../interlocking/InterlockingService.h"

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
    , pollingTimer(std::make_unique<QTimer>(this))
    , connected(false)
{
    connect(pollingTimer.get(), &QTimer::timeout, this, &DatabaseManager::pollDatabase);
    pollingTimer->setInterval(POLLING_INTERVAL_MS);

    // ADD: Health monitoring for notifications
    m_notificationHealthTimer = new QTimer(this);
    connect(m_notificationHealthTimer, &QTimer::timeout, this, &DatabaseManager::checkNotificationHealth);
    m_notificationHealthTimer->start(100000); // Check every minute
}


DatabaseManager::~DatabaseManager() {
    stopPolling();
    if (db.isOpen()) {
        db.close();
    }
    cleanup();
    qDebug() << "DatabaseManager destroyed";
}

bool DatabaseManager::connectToDatabase()
{
    // Try system PostgreSQL first
    if (connectToSystemPostgreSQL()) {
        qDebug() << "Connected to system PostgreSQL";
        enableRealTimeUpdates();  // Enable LISTEN/NOTIFY
        return true;
    }

    qDebug() << "System PostgreSQL unavailable, starting portable mode...";

    // Fall back to portable PostgreSQL
    if (startPortableMode()) {
        qDebug() << "Connected to portable PostgreSQL";
        enableRealTimeUpdates();  // Enable LISTEN/NOTIFY
        return true;
    }

    // Set disconnected state and emit signal
    connected = false;
    m_isConnected = false;
    emit connectionStateChanged(connected);
    emit errorOccurred("Failed to connect to any PostgreSQL instance");
    return false;
}

// In DatabaseManager.cpp - Fix connectToSystemPostgreSQL()
bool DatabaseManager::connectToSystemPostgreSQL()
{
    try {
        // CRITICAL: Check if connection already exists and is open
        if (QSqlDatabase::contains("system_connection")) {
            QSqlDatabase existingDb = QSqlDatabase::database("system_connection");
            if (existingDb.isOpen() && existingDb.isValid()) {
                qDebug() << "Using existing system PostgreSQL connection";
                db = existingDb;
                connected = true;
                m_isConnected = true;
                return true;
            }
            // CRITICAL: Only remove if connection is actually closed
            qDebug() << "Removing stale system connection";
            m_notificationsEnabled = false;
            m_notificationsWorking = false;
            QSqlDatabase::removeDatabase("system_connection");
        }

        db = QSqlDatabase::addDatabase("QPSQL", "system_connection");
        db.setHostName("localhost");
        db.setPort(m_systemPort);
        db.setDatabaseName("railway_control_system");
        db.setUserName("postgres");
        db.setPassword("qwerty");

        if (db.open()) {
            connected = true;
            m_isConnected = true;
            emit connectionStateChanged(connected);
            qDebug() << "Connected to system PostgreSQL";
            return true;
        }
    } catch (...) {
        qDebug() << "System PostgreSQL connection failed";
    }

    connected = false;
    m_isConnected = false;
    emit connectionStateChanged(connected);
    return false;
}

bool DatabaseManager::startPortableMode()
{
    m_appDirectory = getApplicationDirectory();
    m_postgresPath = m_appDirectory + "/database/postgresql";
    m_dataPath = m_appDirectory + "/database/data";

    // Initialize database if needed
    if (!QDir(m_dataPath).exists()) {
        if (!initializePortableDatabase()) {
            return false;
        }
    }

    // Check if server is already running before starting
    if (!isPortableServerRunning()) {
        if (!startPortablePostgreSQL()) {
            return false;
        }
        // Wait for server to start
        // QThread::sleep(1);
    } else {
        qDebug() << "Portable PostgreSQL server already running";
    }

    // Remove existing connection if it exists
    if (QSqlDatabase::contains("portable_connection")) {
        m_notificationsEnabled = false;
        m_notificationsWorking = false;
        QSqlDatabase::removeDatabase("portable_connection");
    }

    try {
        db = QSqlDatabase::addDatabase("QPSQL", "portable_connection");
        db.setHostName("localhost");
        db.setPort(m_portablePort);
        db.setDatabaseName("railway_control_system");
        db.setUserName("postgres");
        db.setPassword("qwerty");

        if (db.open()) {
            connected = true;
            m_isConnected = true;
            m_connectionStatus = "Connected to Portable PostgreSQL";
            emit connectionStateChanged(connected);
            qDebug() << "Portable PostgreSQL connected with schema created";
            return true;
        }
    } catch (const std::exception& e) {
        qDebug() << "Portable PostgreSQL connection failed:" << e.what();
    }

    connected = false;
    m_isConnected = false;
    emit connectionStateChanged(connected);
    return false;
}

bool DatabaseManager::initializePortableDatabase()
{
    QString initdbPath = m_postgresPath + "/bin/initdb.exe";

    if (!QFile::exists(initdbPath)) {
        qDebug() << "PostgreSQL binaries not found at:" << m_postgresPath;
        return false;
    }

    QProcess initProcess;
    QStringList arguments;
    arguments << "-D" << m_dataPath
              << "-U" << "postgres"      // CHANGED: Use postgres user
              << "-A" << "trust"         // Start with trust, convert later
              << "-E" << "UTF8";

    qDebug() << "🔧 Initializing portable database with postgres user...";
    initProcess.start(initdbPath, arguments);

    if (!initProcess.waitForFinished(100)) {
        qDebug() << "Database initialization timed out";
        return false;
    }

    if (initProcess.exitCode() != 0) {
        qDebug() << "Database initialization failed:" << initProcess.readAllStandardError();
        return false;
    }

    qDebug() << "Portable database initialized with postgres user";
    return true;
}

bool DatabaseManager::startPortablePostgreSQL()
{
    QString pgCtlPath = m_postgresPath + "/bin/pg_ctl.exe";
    QString logPath = m_appDirectory + "/database/logs/postgresql.log";

    // Ensure logs directory exists
    QDir().mkpath(QFileInfo(logPath).path());

    if (m_postgresProcess) {
        delete m_postgresProcess;
    }

    m_postgresProcess = new QProcess(this);

    QStringList arguments;
    arguments << "-D" << m_dataPath
              << "-l" << logPath
              << "start";  // REMOVED: -o port argument (port is in postgresql.conf)

    qDebug() << "🚀 Starting portable PostgreSQL server...";
    qDebug() << "Command:" << pgCtlPath << arguments.join(" ");

    m_postgresProcess->start(pgCtlPath, arguments);

    if (!m_postgresProcess->waitForFinished(100)) {  // Increased timeout
        qDebug() << "Failed to start PostgreSQL server (timeout)";
        return false;
    }

    if (m_postgresProcess->exitCode() != 0) {
        QString errorOutput = m_postgresProcess->readAllStandardError();
        QString standardOutput = m_postgresProcess->readAllStandardOutput();
        qDebug() << "PostgreSQL server start failed with exit code:" << m_postgresProcess->exitCode();
        qDebug() << "Error output:" << errorOutput;
        qDebug() << "Standard output:" << standardOutput;
        return false;
    }

    qDebug() << "Portable PostgreSQL server started on port" << m_portablePort;
    return true;
}

QString DatabaseManager::getApplicationDirectory()
{
    // Go up one level from app/ to get to the root project directory
    QDir appDir(QCoreApplication::applicationDirPath());
    appDir.cdUp();  // Go from "app/" to root directory
    return appDir.absolutePath();
}

void DatabaseManager::cleanup()
{
    if (m_postgresProcess) {
        stopPortablePostgreSQL();
        delete m_postgresProcess;
        m_postgresProcess = nullptr;
    }
}

bool DatabaseManager::stopPortablePostgreSQL()
{
    if (!m_postgresProcess) return true;

    QString pgCtlPath = m_postgresPath + "/bin/pg_ctl.exe";

    QProcess stopProcess;
    QStringList arguments;
    arguments << "-D" << m_dataPath << "stop";

    qDebug() << "Stopping portable PostgreSQL server...";
    stopProcess.start(pgCtlPath, arguments);

    if (stopProcess.waitForFinished(5000)) {
        qDebug() << "PostgreSQL server stopped successfully";
        return true;
    }

    qDebug() << "PostgreSQL server stop timed out";
    return false;
}

void DatabaseManager::enableRealTimeUpdates() {
    if (m_notificationsEnabled) {
        qDebug() << "Real-time updates already enabled";
        return;
    }

    if (!connected || !db.isOpen()) {
        qWarning() << "Cannot enable real-time updates - database not connected";
        return;
    }

    // Check if driver supports notifications
    if (!db.driver()->hasFeature(QSqlDriver::EventNotifications)) {
        qWarning() << "Database driver does not support event notifications";
        return;
    }

    // Use subscribeToNotification
    if (db.driver()->subscribeToNotification("railway_changes")) {
        qDebug() << "Subscribed to railway_changes notifications";

        // ENHANCED: Connect with health tracking
        QObject::connect(db.driver(), &QSqlDriver::notification,
                         this, [this](const QString& name, QSqlDriver::NotificationSource source, const QVariant& payload) {
                             // TRACK SEGMENT: Update health indicators
                             m_lastNotificationReceived = QDateTime::currentDateTime();
                             m_notificationsWorking = true;

                             qDebug() << "🔔 NOTIFICATION RECEIVED:" << name << "Payload:" << payload.toString();
                             this->handleDatabaseNotification(name, payload);

                             // HYBRID: Reduce polling frequency
                             if (pollingTimer->interval() != POLLING_INTERVAL_SLOW) {
                                 pollingTimer->setInterval(POLLING_INTERVAL_SLOW);
                                 qDebug() << "Reduced polling to" << POLLING_INTERVAL_SLOW << "ms - notifications working";
                             }
                         });

        m_notificationsEnabled = true;
        m_lastNotificationReceived = QDateTime::currentDateTime(); // Initialize

        // Send test notification
        QSqlQuery testQuery(db);
        if (testQuery.exec("SELECT pg_notify('railway_changes', "
                           "'{\"test\": \"startup\", \"timestamp\": \"" +
                           QString::number(QDateTime::currentSecsSinceEpoch()) + "\"}'::text)")) {
            qDebug() << "Test notification sent";
        }
    } else {
        qWarning() << "Failed to subscribe to railway_changes notifications";
    }
}

void DatabaseManager::checkNotificationHealth() {
    if (!m_notificationsEnabled) return;

    QDateTime now = QDateTime::currentDateTime();

    if (m_lastNotificationReceived.isValid() &&
        m_lastNotificationReceived.secsTo(now) > 300) {

        qWarning() << "No notifications for 1 seconds - assuming failure";
        m_notificationsWorking = false;

        // UPDATE: Emit signal when changing interval
        pollingTimer->setInterval(POLLING_INTERVAL_FAST);
        emit pollingIntervalChanged(POLLING_INTERVAL_FAST); // ADD

        qDebug() << "Increased polling to" << POLLING_INTERVAL_FAST << "ms (notification failover)";
    }
}

void DatabaseManager::handleDatabaseNotification(const QString& name, const QVariant& payload) {
    qDebug() << "🔔 NOTIFICATION HANDLER CALLED:" << name << payload.toString();

    if (name != "railway_changes") {
        qDebug() << "Unexpected notification channel:" << name;
        return;
    }

    QString payloadStr = payload.toString();
    if (payloadStr.isEmpty()) {
        qWarning() << "Empty notification payload";
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(payloadStr.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << parseError.errorString() << "Payload:" << payloadStr;
        return;
    }

    QJsonObject obj = doc.object();
    QString table = obj["table"].toString();
    QString operation = obj["operation"].toString();
    QString entityId = obj["entity_id"].toString();

    qDebug() << "Parsed notification:" << table << operation << entityId;

    // ADD: Update polling interval when notifications are working
    if (pollingTimer && pollingTimer->isActive() && pollingTimer->interval() != POLLING_INTERVAL_SLOW) {
        pollingTimer->setInterval(POLLING_INTERVAL_SLOW);
        emit pollingIntervalChanged(POLLING_INTERVAL_SLOW);
        qDebug() << "Reduced polling to" << POLLING_INTERVAL_SLOW << "ms - notifications working";
    }

    // UPDATE: Mark notifications as working (for health monitoring)
    m_notificationsWorking = true;
    m_lastNotificationReceived = QDateTime::currentDateTime();

    // SAFETY: No cache refreshing - just emit signals for UI updates
    if (obj["test"].toString() == "startup") {
        qDebug() << "Test notification received - system working";
        return; // ← Don't trigger data refresh
    }

    if (table == "signals") {
        emit signalsChanged();
        emit signalUpdated(entityId);
        qDebug() << "Emitted signalsChanged and signalUpdated(" << entityId << ")";
    } else if (table == "point_machines") {
        emit pointMachinesChanged();
        emit pointMachineUpdated(entityId);
        qDebug() << "Emitted pointMachinesChanged and pointMachineUpdated(" << entityId << ")";
    } else if (table == "track_segments") {
        emit trackSegmentsChanged();  // FIXED: Consistent naming
        emit trackSegmentUpdated(entityId);  // FIXED: Consistent naming
        qDebug() << "Emitted trackSegmentsChanged and trackSegmentUpdated(" << entityId << ")";
    } else if (table == "track_circuits") {  // NEW: Handle circuit notifications
        emit trackCircuitsChanged();
        emit trackSegmentsChanged(); // Segments depend on circuits
        qDebug() << "Emitted trackCircuitsChanged and trackSegmentsChanged (circuit affects segments)";
    }

    emit dataUpdated();
    qDebug() << "Emitted dataUpdated()";
}

int DatabaseManager::getCurrentPollingInterval() const {
    // ADD: Debug logging
    qDebug() << "getCurrentPollingInterval() called:";
    qDebug() << "   pollingTimer exists:" << (pollingTimer != nullptr);
    if (pollingTimer) {
        qDebug() << "   pollingTimer->isActive():" << pollingTimer->isActive();
        qDebug() << "   pollingTimer->interval():" << pollingTimer->interval();
    }

    if (!pollingTimer || !pollingTimer->isActive()) {
        qDebug() << "   → Returning 0 (Not polling)";
        return 0; // Not polling
    }

    int interval = pollingTimer->interval();
    qDebug() << "   → Returning interval:" << interval;
    return interval;
}

QString DatabaseManager::getPollingIntervalDisplay() const {
    int interval = getCurrentPollingInterval();

    // ADD: Debug logging
    qDebug() << "getPollingIntervalDisplay() called:";
    qDebug() << "   interval from getCurrentPollingInterval():" << interval;

    if (interval == 0) {
        qDebug() << "   → Returning 'Not polling'";
        return "Not polling";
    } else if (interval < 1000) {
        QString result = QString("%1ms").arg(interval);
        qDebug() << "   → Returning:" << result;
        return result;
    } else if (interval < 60000) {
        QString result = QString("%1s").arg(interval / 1000);
        qDebug() << "   → Returning:" << result;
        return result;
    } else {
        int minutes = interval / 60000;
        int seconds = (interval % 60000) / 1000;
        QString result;
        if (seconds == 0) {
            result = QString("%1m").arg(minutes);
        } else {
            result = QString("%1m %2s").arg(minutes).arg(seconds);
        }
        qDebug() << "   → Returning:" << result;
        return result;
    }
}

void DatabaseManager::startPolling() {
    if (connected) {
        // INTELLIGENT: Longer interval when notifications are working
        int interval = m_notificationsWorking ? POLLING_INTERVAL_SLOW : POLLING_INTERVAL_FAST;
        pollingTimer->setInterval(interval);
        pollingTimer->start();

        emit pollingIntervalChanged(interval);

        qDebug() << "HYBRID: Database polling started"
                 << "(interval:" << interval << "ms)"
                 << "Notifications working:" << m_notificationsWorking;
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

    qDebug() << "SAFETY POLLING: Direct database state check";
    detectAndEmitChanges();
    emit dataUpdated(); // Trigger QML property updates
}

void DatabaseManager::detectAndEmitChanges() {
    // Poll signals
    QSqlQuery signalQuery("SELECT signal_id, current_aspect_id FROM railway_control.signals", db);
    while (signalQuery.next()) {
        QString signalId = signalQuery.value(0).toString();
        int aspectId = signalQuery.value(1).toInt();

        if (!lastSignalStates.contains(signalId.toInt()) || lastSignalStates[signalId.toInt()] != QString::number(aspectId)) {
            lastSignalStates[signalId.toInt()] = QString::number(aspectId);
            emit signalStateChanged(signalId.toInt(), QString::number(aspectId));
        }
    }

    // FIXED: Poll trackSegment circuits for occupancy (not segments)
    QSqlQuery circuitQuery("SELECT circuit_id, is_occupied FROM railway_control.track_circuits", db);
    while (circuitQuery.next()) {
        QString circuitId = circuitQuery.value(0).toString();
        bool isOccupied = circuitQuery.value(1).toBool();

        // Use circuit_id as key for tracking state changes
        int circuitKey = qHash(circuitId);
        if (!lastTrackSegmentStates.contains(circuitKey) || lastTrackSegmentStates[circuitKey] != isOccupied) {
            lastTrackSegmentStates[circuitKey] = isOccupied;
            emit trackCircuitStateChanged(circuitKey, isOccupied);
        }
    }
}

bool DatabaseManager::isPortableServerRunning()
{
    QString pgCtlPath = m_postgresPath + "/bin/pg_ctl.exe";

    QProcess checkProcess;
    QStringList arguments;
    arguments << "-D" << m_dataPath << "status";

    checkProcess.start(pgCtlPath, arguments);
    checkProcess.waitForFinished(100);

    // If exit code is 0, server is running
    bool isRunning = (checkProcess.exitCode() == 0);
    qDebug() << "Portable PostgreSQL server running check:" << isRunning;

    return isRunning;
}

// SAFETY: Direct database queries - NO CACHING
QVariantList DatabaseManager::getTrackSegmentsList() {
    if (!connected) return QVariantList();

    qDebug() << "?? SAFETY: getTrackSegmentsList() - DIRECT DATABASE QUERY";

    QVariantList trackSegments;
    QSqlQuery trackSegmentQuery(db);
    // ? Use the view that joins with circuits for occupancy
    QString trackSegmentSql = R"(
        SELECT segment_id, segment_name, start_row, start_col, end_row, end_col,
               track_segment_type, is_occupied, is_assigned, occupied_by, is_active, circuit_id
        FROM railway_control.v_track_segments_with_occupancy
        ORDER BY segment_id
    )";

    if (trackSegmentQuery.exec(trackSegmentSql)) {
        while (trackSegmentQuery.next()) {
            trackSegments.append(convertTrackSegmentRowToVariant(trackSegmentQuery));
        }
    } else {
        qWarning() << "? SAFETY CRITICAL: Track Segment query failed:" << trackSegmentQuery.lastError().text();
    }

    return trackSegments;
}

QVariantList DatabaseManager::getAllSignalsList() {
    if (!connected) return QVariantList();

    QVariantList signalsList;
    QSqlQuery signalQuery(db);

    // ✅ SIMPLIFIED: Use the enhanced view instead of complex joins
    QString signalSql = R"(
        SELECT
            signal_id,
            signal_name,
            signal_type,
            location_row as row,
            location_col as col,
            direction,
            current_aspect,
            current_aspect_name,
            current_aspect_color,
            calling_on_aspect,
            calling_on_aspect_name,
            calling_on_aspect_color,
            loop_aspect,
            loop_aspect_name,
            loop_aspect_color,
            loop_signal_configuration,
            aspect_count,
            possible_aspects,
            is_active,
            location_description as location,
            last_changed_at,
            last_changed_by
        FROM railway_control.v_signals_complete
        ORDER BY signal_id
    )";

    if (signalQuery.exec(signalSql)) {
        while (signalQuery.next()) {
            signalsList.append(convertSignalRowToVariant(signalQuery));
        }
        qDebug() << "✅ Loaded" << signalsList.size() << "signals with complete aspect information from view";
    } else {
        qWarning() << "❌ SAFETY CRITICAL: Enhanced signal view query failed:" << signalQuery.lastError().text();
    }

    return signalsList;
}

QVariantList DatabaseManager::getAllPointMachinesList() {
    if (!connected) return QVariantList();

    qDebug() << "SAFETY: getAllPointMachinesList() - DIRECT DATABASE QUERY from getAllPointMachinesList()";

    QVariantList points;
    QSqlQuery pointQuery(db);
    QString pointSql = R"(
        SELECT pm.machine_id, pm.machine_name, pm.junction_row, pm.junction_col,
               pm.root_track_segment_connection, pm.normal_track_segment_connection, pm.reverse_track_segment_connection,
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
        qWarning() << "SAFETY CRITICAL: Point machine query failed:" << pointQuery.lastError().text();
    }

    return points;
}

QVariantList DatabaseManager::getTextLabelsList() {
    if (!connected) return QVariantList();

    qDebug() << "SAFETY: getTextLabelsList() - DIRECT DATABASE QUERY";

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
        qWarning() << "SAFETY CRITICAL: Text label query failed:" << labelQuery.lastError().text();
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

// SAFETY: Individual object queries - DIRECT DATABASE
QVariantMap DatabaseManager::getSignalById(const QString& signalId) {
    if (!connected) return QVariantMap();

    qDebug() << "SAFETY: getSignalById(" << signalId << ") - QUERYING COMPLETE SIGNAL VIEW";

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT
            signal_id,
            signal_name,
            signal_type,
            signal_type_name,
            location_row as row,
            location_col as col,
            direction,
            current_aspect,
            current_aspect_name,
            current_aspect_color,
            calling_on_aspect,
            calling_on_aspect_name,
            calling_on_aspect_color,
            loop_aspect,
            loop_aspect_name,
            loop_aspect_color,
            loop_signal_configuration,
            aspect_count,
            possible_aspects,
            is_active,
            location_description as location,
            last_changed_at,
            last_changed_by
        FROM railway_control.v_signals_complete
        WHERE signal_id = ?
    )");

    query.addBindValue(signalId);

    if (query.exec() && query.next()) {
        return convertSignalRowToVariant(query);
    }

    qWarning() << "SAFETY: Signal" << signalId << "not found in complete view";
    return QVariantMap();
}

QVariantMap DatabaseManager::getTrackSegmentById(const QString& trackSegmentId) {
    if (!connected) return QVariantMap();

    qDebug() << "?? QUERY: getTrackSegmentById(" << trackSegmentId << ")";

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT segment_id, segment_name, start_row, start_col, end_row, end_col,
               track_segment_type, is_occupied, is_assigned, occupied_by, is_active, circuit_id
        FROM railway_control.v_track_segments_with_occupancy
        WHERE segment_id = ?
    )");
    query.addBindValue(trackSegmentId);

    if (query.exec() && query.next()) {
        return convertTrackSegmentRowToVariant(query);
    }

    qWarning() << "? Track segment" << trackSegmentId << "not found";
    return QVariantMap();
}

QVariantMap DatabaseManager::getPointMachineById(const QString& machineId) {
    if (!connected) return QVariantMap();

    qDebug() << "SAFETY: getPointMachineById(" << machineId << ") - DIRECT DATABASE QUERY";

    QSqlQuery query(db);

    // FIXED: Added paired_entity to SELECT statement
    query.prepare(R"(
        SELECT pm.machine_id, pm.machine_name, pm.junction_row, pm.junction_col,
               pm.root_track_segment_connection, pm.normal_track_segment_connection, pm.reverse_track_segment_connection,
               pp.position_code as position, pm.operating_status, pm.transition_time_ms, pm.paired_entity
        FROM railway_control.point_machines pm
        LEFT JOIN railway_config.point_positions pp ON pm.current_position_id = pp.id
        WHERE pm.machine_id = ?
    )");
    query.addBindValue(machineId);

    if (query.exec() && query.next()) {
        return convertPointMachineRowToVariant(query);
    } else {
        qWarning() << "Failed to get point machine:" << machineId << query.lastError().text();
    }

    return QVariantMap();
}

QVariantList DatabaseManager::getPointMachinesList() {
    if (!connected) return QVariantList();

    qDebug() << "SAFETY: getAllPointMachinesList() - DIRECT DATABASE QUERY from getAllPointMachinesList()";

    QVariantList points;
    QSqlQuery pointQuery(db);

    // FIXED: Added paired_entity to SELECT statement
    QString pointSql = R"(
        SELECT pm.machine_id, pm.machine_name, pm.junction_row, pm.junction_col,
               pm.root_track_segment_connection, pm.normal_track_segment_connection, pm.reverse_track_segment_connection,
               pp.position_code as position, pm.operating_status, pm.transition_time_ms, pm.paired_entity
        FROM railway_control.point_machines pm
        LEFT JOIN railway_config.point_positions pp ON pm.current_position_id = pp.id
        ORDER BY pm.machine_id
    )";

    if (pointQuery.exec(pointSql)) {
        while (pointQuery.next()) {
            points.append(convertPointMachineRowToVariant(pointQuery));
        }
    } else {
        qWarning() << "SAFETY CRITICAL: Point machine query failed:" << pointQuery.lastError().text();
    }

    return points;
}

bool DatabaseManager::updateMainSignalAspect(const QString& signalId, const QString& newAspect) {
    if (!connected) return false;

    QElapsedTimer timer;
    timer.start();

    qDebug() << "SAFETY: Updating MAIN signal aspect:" << signalId << "to aspect:" << newAspect;

    // Get current main aspect for interlocking validation
    QString currentAspect = getCurrentSignalAspect(signalId);
    if (currentAspect.isEmpty()) {
        qWarning() << "Could not get current main aspect for signal:" << signalId;
        emit operationBlocked(signalId, "Signal not found or invalid main aspect state");
        return false;
    }

    // Main signal interlocking validation
    if (m_interlockingService) {
        auto validation = m_interlockingService->validateMainSignalOperation(
            signalId, currentAspect, newAspect, "HMI_USER");

        if (!validation.isAllowed()) {
            qDebug() << "Main signal operation blocked by interlocking:" << validation.getReason();
            emit operationBlocked(signalId, validation.getReason());
            return false;
        }

        qDebug() << "Main signal interlocking validation passed for signal" << signalId;
    } else {
        qWarning() << "Interlocking service not available - proceeding without validation";
    }

    // Database transaction for main signal update
    QSqlQuery query(db);

    if (!db.transaction()) {
        qWarning() << "Failed to start transaction for main signal:" << db.lastError().text();
        return false;
    }

    // Call existing main signal update function
    query.prepare("SELECT railway_control.update_signal_aspect(?, ?, 'HMI_USER')");
    query.addBindValue(signalId);
    query.addBindValue(newAspect);

    bool success = false;
    if (query.exec() && query.next()) {
        success = query.value(0).toBool();
        if (success && db.commit()) {
            // Verify main aspect change
            QSqlQuery verifyQuery(db);
            verifyQuery.prepare("SELECT current_aspect_id FROM railway_control.signals WHERE signal_id = ?");
            verifyQuery.addBindValue(signalId);
            if (verifyQuery.exec() && verifyQuery.next()) {
                int currentAspectId = verifyQuery.value(0).toInt();
                qDebug() << "SAFETY: Main signal" << signalId << "now has aspect_id:" << currentAspectId;
            }

            // Emit success signals
            emit signalUpdated(signalId);
            emit signalsChanged();

            qDebug() << "Main signal operation completed in" << timer.elapsed() << "ms";
            return true;
        } else {
            qWarning() << "Main signal update failed:" << query.lastError().text();
            db.rollback();
            return false;
        }
    } else {
        qWarning() << "Main signal query execution failed:" << query.lastError().text();
        db.rollback();
        return false;
    }
}

bool DatabaseManager::updateSubsidiarySignalAspect(const QString& signalId,
                                                   const QString& aspectType,
                                                   const QString& newAspect) {
    if (!connected) return false;

    QElapsedTimer timer;
    timer.start();

    qDebug() << "SAFETY: Updating SUBSIDIARY signal aspect:" << signalId
             << "type:" << aspectType << "to aspect:" << newAspect;

    // Validate aspect type
    if (aspectType != "CALLING_ON" && aspectType != "LOOP") {
        qWarning() << "Invalid subsidiary aspect type:" << aspectType;
        emit operationBlocked(signalId, "Invalid subsidiary signal type: " + aspectType);
        return false;
    }

    // Get current subsidiary aspect for validation
    QString currentSubsidiaryAspect = getCurrentSubsidiaryAspect(signalId, aspectType);
    if (currentSubsidiaryAspect.isEmpty()) {
        qWarning() << "Could not get current subsidiary aspect for signal:" << signalId << "type:" << aspectType;
        emit operationBlocked(signalId, "Signal not found or invalid subsidiary aspect state");
        return false;
    }

    // Interlocking validation for subsidiary signals
    if (m_interlockingService) {
        auto validation = m_interlockingService->validateSubsidiarySignalOperation(
            signalId, aspectType, currentSubsidiaryAspect, newAspect, "HMI_USER");

        if (!validation.isAllowed()) {
            qDebug() << "Subsidiary signal operation blocked by interlocking:" << validation.getReason();
            emit operationBlocked(signalId, validation.getReason());
            return false;
        }

        qDebug() << "Subsidiary signal interlocking validation passed for signal" << signalId;
    } else {
        qWarning() << "Interlocking service not available - proceeding without validation";
    }

    // Database transaction for subsidiary signal update
    QSqlQuery query(db);

    if (!db.transaction()) {
        qWarning() << "Failed to start transaction for subsidiary signal:" << db.lastError().text();
        return false;
    }

    // Call subsidiary signal update function (TO BE CREATED)
    query.prepare("SELECT railway_control.update_subsidiary_signal_aspect(?, ?, ?, 'HMI_USER')");
    query.addBindValue(signalId);
    query.addBindValue(aspectType);
    query.addBindValue(newAspect);

    bool success = false;
    if (query.exec() && query.next()) {
        success = query.value(0).toBool();
        if (success && db.commit()) {
            // Verify subsidiary aspect change
            QString columnName = (aspectType == "CALLING_ON") ? "calling_on_aspect" : "loop_aspect";
            QSqlQuery verifyQuery(db);
            verifyQuery.prepare(QString("SELECT %1 FROM railway_control.signals WHERE signal_id = ?").arg(columnName));
            verifyQuery.addBindValue(signalId);
            if (verifyQuery.exec() && verifyQuery.next()) {
                QString currentValue = verifyQuery.value(0).toString();
                qDebug() << "SAFETY: Subsidiary signal" << signalId << aspectType
                         << "now has value:" << currentValue;
            }

            // Emit success signals
            emit signalUpdated(signalId);
            emit signalsChanged();

            qDebug() << "Subsidiary signal operation completed in" << timer.elapsed() << "ms";
            return true;
        } else {
            qWarning() << "Subsidiary signal update failed:" << query.lastError().text();
            db.rollback();
            return false;
        }
    } else {
        qWarning() << "Subsidiary signal query execution failed:" << query.lastError().text();
        db.rollback();
        return false;
    }
}

// SAFETY: Update operations - NO CACHE INVALIDATION
bool DatabaseManager::updateSignalAspect(const QString& signalId,
                                         const QString& aspectType,
                                         const QString& newAspect) {
    if (!connected) {
        qWarning() << "Database not connected - cannot update signal aspect";
        return false;
    }

    qDebug() << "ROUTER: Signal aspect update request:"
             << "Signal:" << signalId
             << "Type:" << aspectType
             << "New aspect:" << newAspect;

    // Validate aspect type parameter
    if (aspectType != "MAIN" && aspectType != "CALLING_ON" && aspectType != "LOOP") {
        qWarning() << "Invalid aspect type:" << aspectType
                   << "Must be 'MAIN', 'CALLING_ON', or 'LOOP'";
        emit operationBlocked(signalId, "Invalid aspect type: " + aspectType);
        return false;
    }

    // Route to appropriate function based on aspect type
    if (aspectType == "MAIN") {
        qDebug() << "ROUTER: Routing to updateMainSignalAspect()";
        return updateMainSignalAspect(signalId, newAspect);
    }
    else if (aspectType == "CALLING_ON" || aspectType == "LOOP") {
        qDebug() << "ROUTER: Routing to updateSubsidiarySignalAspect()";
        return updateSubsidiarySignalAspect(signalId, aspectType, newAspect);
    }

    // Should never reach here due to validation above
    qWarning() << "ROUTER: Unexpected routing failure for aspect type:" << aspectType;
    return false;
}

QString DatabaseManager::getPairedMachine(const QString& machineId) {
    QSqlQuery query(db);
    query.prepare("SELECT paired_entity FROM railway_control.point_machines WHERE machine_id = ?");
    query.addBindValue(machineId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

bool DatabaseManager::updatePointMachinePosition(const QString& machineId, const QString& newPosition) {
    if (!connected) return false;

    qDebug() << "SAFETY: Updating point machine:" << machineId << "to position:" << newPosition;

    // Step 1: Get current positions for paired validation
    QString currentPosition = getCurrentPointPosition(machineId);
    if (currentPosition.isEmpty()) {
        qWarning() << "Could not get current position for point machine:" << machineId;
        emit operationBlocked(machineId, "Point machine not found or invalid state");
        return false;
    }

    // Step 2: Get paired machine info for comprehensive validation
    QString pairedMachineId = getPairedMachine(machineId);

    if (!pairedMachineId.isEmpty()) {
        QString pairedCurrentPosition = getCurrentPointPosition(pairedMachineId);

        // === USE PAIRED VALIDATION ===
        if (m_interlockingService) {
            auto validation = m_interlockingService->validatePairedPointMachineOperation(
                machineId, pairedMachineId, currentPosition, pairedCurrentPosition, newPosition, "HMI_USER");

            if (!validation.isAllowed()) {
                qDebug() << "Paired point machine operation blocked by interlocking:" << validation.getReason();
                emit operationBlocked(machineId, validation.getReason());
                return false;
            }
        }
    } else {
        // === SINGLE MACHINE VALIDATION ===
        if (m_interlockingService) {
            auto validation = m_interlockingService->validatePointMachineOperation(
                machineId, currentPosition, newPosition, "HMI_USER");

            if (!validation.isAllowed()) {
                qDebug() << "Point machine operation blocked by interlocking:" << validation.getReason();
                emit operationBlocked(machineId, validation.getReason());
                return false;
            }
        }
    }

    qDebug() << "Interlocking validation passed for all affected machines";

    // Step 3: Execute atomic database operation (rest remains unchanged)
    if (!db.transaction()) {
        qWarning() << "SAFETY CRITICAL: Failed to start transaction for point machine update";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT railway_control.update_point_position_paired(?, ?, 'HMI_USER')");
    query.addBindValue(machineId);
    query.addBindValue(newPosition);

    bool success = false;
    if (query.exec() && query.next()) {
        QJsonDocument doc = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
        QJsonObject result = doc.object();

        success = result["success"].toBool();
        bool positionMismatch = result["position_mismatch"].toBool();
        QJsonArray updatedMachines = result["machines_updated"].toArray();
        QString message = result["message"].toString();

        if (success) {
            if (db.commit()) {
                qDebug() << "Point machine update successful:" << message;

                // Emit appropriate signals
                QStringList machinesList;
                for (const auto& machine : updatedMachines) {
                    machinesList.append(machine.toString());
                    emit pointMachineUpdated(machine.toString());
                }

                if (machinesList.size() > 1) {
                    emit pairedMachinesUpdated(machinesList);
                }

                if (positionMismatch) {
                    qCritical() << "SAFETY WARNING: Position mismatch corrected for paired machines:"
                                << machineId << "and" << pairedMachineId;
                    emit positionMismatchCorrected(machineId, pairedMachineId);
                }

                emit pointMachinesChanged();
                return true;
            } else {
                qWarning() << "SAFETY CRITICAL: Failed to commit transaction:" << db.lastError().text();
                db.rollback();
                return false;
            }
        } else {
            qWarning() << "SAFETY CRITICAL: Point machine update failed:" << message;
            db.rollback();
            return false;
        }
    }

    qWarning() << "SAFETY CRITICAL: Point machine update query failed:" << query.lastError().text();
    db.rollback();
    return false;
}

QString DatabaseManager::getCurrentSignalAspect(const QString& signalId) {
    if (!connected) {
        qWarning() << "Database not connected - cannot get signal aspect";
        return QString();
    }

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT sa.aspect_code
        FROM railway_control.signals s
        LEFT JOIN railway_config.signal_aspects sa ON s.current_aspect_id = sa.id
        WHERE s.signal_id = ?
    )");
    query.addBindValue(signalId);

    if (!query.exec()) {
        qWarning() << "Failed to get current aspect for signal" << signalId << ":" << query.lastError().text();
        return QString();
    }

    if (query.next()) {
        return query.value(0).toString();
    }

    qWarning() << "Signal not found:" << signalId;
    return QString();
}

// ✅ ENHANCED: getCurrentSubsidiaryAspect to work with new schema
QString DatabaseManager::getCurrentSubsidiaryAspect(const QString& signalId, const QString& aspectType) {
    if (!connected) return QString();

    QString columnName;
    if (aspectType == "CALLING_ON") {
        columnName = "sa_calling.aspect_code";
    } else if (aspectType == "LOOP") {
        columnName = "sa_loop.aspect_code";
    } else {
        qWarning() << "❌ Invalid subsidiary aspect type:" << aspectType;
        return QString();
    }

    QSqlQuery query(db);
    QString sql = QString(R"(
        SELECT COALESCE(%1, 'OFF') as aspect_code
        FROM railway_control.signals s
        LEFT JOIN railway_config.signal_aspects sa_calling ON s.calling_on_aspect_id = sa_calling.id
        LEFT JOIN railway_config.signal_aspects sa_loop ON s.loop_aspect_id = sa_loop.id
        WHERE s.signal_id = ?
    )").arg(columnName);

    query.prepare(sql);
    query.addBindValue(signalId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    qWarning() << "❌ Failed to get current subsidiary aspect:" << query.lastError().text();
    return QString();
}

QString DatabaseManager::getCurrentPointPosition(const QString& machineId) {
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT pp.position_code
        FROM railway_control.point_machines pm
        LEFT JOIN railway_config.point_positions pp ON pm.current_position_id = pp.id
        WHERE pm.machine_id = ?
    )");
    query.addBindValue(machineId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

QStringList DatabaseManager::getProtectedTrackSegments(const QString& signalId) {
    QSqlQuery query(db);
    query.prepare("SELECT protected_track_segment_id FROM railway_control.signal_track_segment_protection WHERE signal_id = ? AND is_active = TRUE");
    query.addBindValue(signalId);

    QStringList trackSegments;
    if (query.exec()) {
        while (query.next()) {
            trackSegments.append(query.value(0).toString());
        }
    }

    return trackSegments;
}

QStringList DatabaseManager::getInterlockedSignals(const QString& signalId) {
    auto signalData = getSignalById(signalId);
    if (!signalData.isEmpty()) {
        return signalData["interlockedWith"].toStringList();
    }
    return QStringList();
}

void DatabaseManager::setInterlockingService(InterlockingService* service) {
    m_interlockingService = service;
    qDebug() << "Interlocking service connected to DatabaseManager";
}

// ADD: Database access method for interlocking branches
QSqlDatabase DatabaseManager::getDatabase() const {
    return db;
}


bool DatabaseManager::updateTrackSegmentOccupancy(const QString& trackSegmentId, bool isOccupied) {
    if (!connected) return false;

    qDebug() << "HARDWARE: Track segment occupancy change:" << trackSegmentId << "→" << isOccupied;
    qDebug() << "             (This updates the CIRCUIT that contains this segment)";

    // Get previous state for interlocking comparison
    bool wasOccupied = false;
    auto currentTrackSegmentData = getTrackSegmentById(trackSegmentId);
    if (!currentTrackSegmentData.isEmpty()) {
        wasOccupied = currentTrackSegmentData["occupied"].toBool();
    }

    // UPDATED: Use the wrapper function that maps segment to circuit
    QSqlQuery query(db);
    query.prepare("SELECT railway_control.update_track_segment_occupancy(?, ?, NULL, 'HARDWARE_AUTO')");
    query.addBindValue(trackSegmentId);
    query.addBindValue(isOccupied);

    if (query.exec() && query.next()) {
        bool success = query.value(0).toBool();
        if (success) {
            // REACTIVE: Trigger automatic interlocking enforcement
            if (m_interlockingService && m_interlockingService->isOperational()) {
                QMetaObject::invokeMethod(m_interlockingService,
                                          "reactToTrackSegmentOccupancyChange", Qt::QueuedConnection,
                                          Q_ARG(QString, trackSegmentId),
                                          Q_ARG(bool, wasOccupied),
                                          Q_ARG(bool, isOccupied));
            }

            emit trackSegmentUpdated(trackSegmentId);  // FIXED: Consistent naming
            emit trackSegmentsChanged();               // FIXED: Consistent naming
        }
        return success;
    }

    qCritical() << "HARDWARE FAILURE: Track segment occupancy update failed:" << query.lastError().text();
    return false;
}

bool DatabaseManager::updateTrackCircuitOccupancy(const QString& trackCircuitId, bool isOccupied) {
    if (!connected) return false;

    qDebug() << "CIRCUIT: Track Segment circuit occupancy change:" << trackCircuitId << "→" << isOccupied;

    QSqlQuery query(db);
    query.prepare("SELECT railway_control.update_track_segment_circuit_occupancy(?, ?, NULL, 'HARDWARE_AUTO')");
    query.addBindValue(trackCircuitId);
    query.addBindValue(isOccupied);

    if (query.exec() && query.next()) {
        bool success = query.value(0).toBool();
        if (success) {
            emit trackCircuitsChanged();  // NEW: Circuit-specific signal
            emit trackSegmentsChanged();  // Also update segments since they depend on circuits
        }
        return success;
    }

    qCritical() << "CIRCUIT FAILURE: Track Segment circuit occupancy update failed:" << query.lastError().text();
    return false;
}

bool DatabaseManager::getTrackCircuitOccupancy(const QString& trackCircuitId) {
    QSqlQuery query(db);
    query.prepare("SELECT is_occupied FROM railway_control.track_circuits WHERE circuit_id = ?");
    query.addBindValue(trackCircuitId);
    if (query.exec() && query.next()) {
        return query.value(0).toBool();
    }
    return false; // Safe default
}

QVariantList DatabaseManager::getTrackSegmentsByCircuitId(const QString& trackCircuitId) {
    if (!connected) return QVariantList();

    qDebug() << "QUERY: getTrackSegmentsByCircuitId(" << trackCircuitId << ")";

    QVariantList segments;
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT segment_id, segment_name, start_row, start_col, end_row, end_col,
               track_segment_type, is_occupied, is_assigned, occupied_by, is_active, circuit_id
        FROM railway_control.v_track_segments_with_occupancy
        WHERE circuit_id = ?
        ORDER BY segment_id
    )");
    query.addBindValue(trackCircuitId);

    if (query.exec()) {
        while (query.next()) {
            segments.append(convertTrackSegmentRowToVariant(query));
        }
    } else {
        qWarning() << "Failed to get segments for circuit" << trackCircuitId << ":" << query.lastError().text();
    }

    return segments;
}

QVariantList DatabaseManager::getTrackCircuitsList() {
    if (!connected) return QVariantList();

    qDebug() << "SAFETY: getTrackCircuitsList() - DIRECT DATABASE QUERY";

    QVariantList circuits;
    QSqlQuery query(db);
    QString sql = R"(
        SELECT circuit_id, circuit_name, is_occupied, occupied_by,
               length_meters, max_speed_kmh, is_active, protecting_signals
        FROM railway_control.track_circuits
        ORDER BY circuit_id
    )";

    if (query.exec(sql)) {
        while (query.next()) {
            QVariantMap circuit;
            circuit["id"] = query.value("circuit_id").toString();
            circuit["name"] = query.value("circuit_name").toString();
            circuit["occupied"] = query.value("is_occupied").toBool();
            circuit["occupiedBy"] = query.value("occupied_by").toString();
            circuit["lengthMeters"] = query.value("length_meters").toDouble();
            circuit["maxSpeedKmh"] = query.value("max_speed_kmh").toInt();
            circuit["isActive"] = query.value("is_active").toBool();

            // Handle protecting signals array
            QString protectingSignalsStr = query.value("protecting_signals").toString();
            if (!protectingSignalsStr.isEmpty()) {
                protectingSignalsStr = protectingSignalsStr.mid(1, protectingSignalsStr.length() - 2); // Remove { }
                circuit["protectingSignals"] = protectingSignalsStr.split(",");
            } else {
                circuit["protectingSignals"] = QStringList();
            }

            circuits.append(circuit);
        }
    } else {
        qWarning() << "SAFETY CRITICAL: Track Segment circuits query failed:" << query.lastError().text();
    }

    return circuits;
}


// bool DatabaseManager::updateTrackSegmentAssignment(const QString& segmentId, bool isAssigned) {
//     if (!connected) return false;

//     qDebug() << "SAFETY: Updating trackSegment assignment:" << segmentId << "to" << isAssigned;

//     QSqlQuery query(db);
//     query.prepare("SELECT railway_control.update_track_segment_assignment(?, ?, 'HMI_USER')");
//     query.addBindValue(segmentId);
//     query.addBindValue(isAssigned);

//     if (query.exec() && query.next()) {
//         bool success = query.value(0).toBool();
//         if (success) {
//             // SAFETY: No cache invalidation - just emit signals
//             emit trackSegmentUpdated(segmentId);
//             emit trackSegmentsChanged();
//         }
//         return success;
//     }

//     qWarning() << "SAFETY CRITICAL: Track Segment assignment update failed:" << query.lastError().text();
//     return false;
// }

// SAFETY: Row conversion helpers (unchanged)
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

QVariantMap DatabaseManager::convertTrackSegmentRowToVariant(const QSqlQuery& query) {
    QVariantMap trackSegment;
    trackSegment["id"] = query.value("segment_id").toString();
    trackSegment["name"] = query.value("segment_name").toString();
    trackSegment["startRow"] = query.value("start_row").toDouble();
    trackSegment["startCol"] = query.value("start_col").toDouble();
    trackSegment["endRow"] = query.value("end_row").toDouble();
    trackSegment["endCol"] = query.value("end_col").toDouble();
    trackSegment["trackSegmentType"] = query.value("track_segment_type").toString();
    trackSegment["occupied"] = query.value("is_occupied").toBool();  // Now from circuit via view
    trackSegment["assigned"] = query.value("is_assigned").toBool();
    trackSegment["occupiedBy"] = query.value("occupied_by").toString();
    trackSegment["isActive"] = query.value("is_active").toBool();
    trackSegment["circuitId"] = query.value("circuit_id").toString();  // NEW: Include circuit_id

    return trackSegment;
}

QVariantMap DatabaseManager::convertPointMachineRowToVariant(const QSqlQuery& query) {
    QVariantMap pm;
    pm["id"] = query.value("machine_id").toString();
    pm["name"] = query.value("machine_name").toString();
    pm["position"] = query.value("position").toString();
    pm["operatingStatus"] = query.value("operating_status").toString();
    pm["transitionTime"] = query.value("transition_time_ms").toInt();

    // NEW: Add paired entity information with error checking
    if (query.record().contains("paired_entity")) {
        QString pairedEntity = query.value("paired_entity").toString();
        pm["pairedEntity"] = pairedEntity.isEmpty() ? QVariant() : pairedEntity;
        pm["isPaired"] = !pairedEntity.isEmpty();
    } else {
        qWarning() << "paired_entity field not found in query results";
        pm["pairedEntity"] = QVariant();
        pm["isPaired"] = false;
    }

    // Add isActive field - default to true if not present in database
    if (query.record().contains("is_active")) {
        pm["isActive"] = query.value("is_active").toBool();
    } else {
        pm["isActive"] = true; // Default to active if field doesn't exist
    }

    // Junction point
    QVariantMap junctionPoint;
    junctionPoint["row"] = query.value("junction_row").toDouble();
    junctionPoint["col"] = query.value("junction_col").toDouble();
    pm["junctionPoint"] = junctionPoint;

    // Track Segment connections (parse JSON)
    QString rootConnStr = query.value("root_track_segment_connection").toString();
    QString normalConnStr = query.value("normal_track_segment_connection").toString();
    QString reverseConnStr = query.value("reverse_track_segment_connection").toString();

    if (!rootConnStr.isEmpty()) {
        QJsonDocument rootDoc = QJsonDocument::fromJson(rootConnStr.toUtf8());
        pm["rootTrackSegment"] = rootDoc.object().toVariantMap();
    }

    if (!normalConnStr.isEmpty()) {
        QJsonDocument normalDoc = QJsonDocument::fromJson(normalConnStr.toUtf8());
        pm["normalTrackSegment"] = normalDoc.object().toVariantMap();
    }

    if (!reverseConnStr.isEmpty()) {
        QJsonDocument reverseDoc = QJsonDocument::fromJson(reverseConnStr.toUtf8());
        pm["reverseTrackSegment"] = reverseDoc.object().toVariantMap();
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

QVariantMap DatabaseManager::getAllTrackCircuitStates() {
    QVariantMap states;
    QSqlQuery query("SELECT circuit_id, is_occupied FROM railway_control.track_circuits", db);
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
    if (!connected) return false;

    qDebug() << "🔧 Setting up railway control schema...";

    QSqlQuery query(db);

    // Create railway_control schema if it doesn't exist
    if (!query.exec("CREATE SCHEMA IF NOT EXISTS railway_control")) {
        qDebug() << "Failed to create railway_control schema:" << query.lastError().text();
        return false;
    }

    // Create track_segments table
    QString createTrackSegments = R"(
        CREATE TABLE IF NOT EXISTS railway_control.track_segments (
            segment_id SERIAL PRIMARY KEY,
            segment_name VARCHAR(100) NOT NULL,
            start_row INTEGER,
            start_col INTEGER,
            end_row INTEGER,
            end_col INTEGER,
            track_segment_type VARCHAR(50),
            is_occupied BOOLEAN DEFAULT FALSE,
            is_assigned BOOLEAN DEFAULT FALSE,
            occupied_by VARCHAR(100),
            is_active BOOLEAN DEFAULT TRUE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createTrackSegments)) {
        qDebug() << "Failed to create track_segments table:" << query.lastError().text();
        return false;
    }

    // Create signals table
    QString createSignals = R"(
        CREATE TABLE IF NOT EXISTS railway_control.signals (
            signal_id SERIAL PRIMARY KEY,
            signal_name VARCHAR(100) NOT NULL,
            current_aspect_id INTEGER DEFAULT 1,
            position_row INTEGER,
            position_col INTEGER,
            signal_type VARCHAR(50),
            is_active BOOLEAN DEFAULT TRUE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createSignals)) {
        qDebug() << "Failed to create signals table:" << query.lastError().text();
        return false;
    }

    // Create point_machines table
    QString createPointMachines = R"(
        CREATE TABLE IF NOT EXISTS railway_control.point_machines (
            machine_id SERIAL PRIMARY KEY,
            machine_name VARCHAR(100) NOT NULL,
            current_position VARCHAR(20) DEFAULT 'NORMAL',
            position_row INTEGER,
            position_col INTEGER,
            is_active BOOLEAN DEFAULT TRUE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createPointMachines)) {
        qDebug() << "Failed to create point_machines table:" << query.lastError().text();
        return false;
    }

    // Insert some test data
    query.exec("INSERT INTO railway_control.track_segments (segment_name, start_row, start_col, end_row, end_col, track_segment_type) "
               "VALUES ('Track Segment 1', 0, 0, 0, 10, 'MAIN') ON CONFLICT DO NOTHING");

    query.exec("INSERT INTO railway_control.signals (signal_name, current_aspect_id, position_row, position_col, signal_type) "
               "VALUES ('Signal A1', 1, 0, 5, 'HOME') ON CONFLICT DO NOTHING");

    qDebug() << "Railway control schema and tables created successfully";
    return true;
}

void DatabaseManager::logError(const QString& operation, const QSqlError& error) {
    qWarning() << "Database error in" << operation << ":" << error.text();
}
