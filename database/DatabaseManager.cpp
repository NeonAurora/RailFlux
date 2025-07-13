#include "DatabaseManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QThread>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include "../interlocking/InterlockingService.h"

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
    , pollingTimer(std::make_unique<QTimer>(this))
    , connected(false)
    , m_connectionTimer(std::make_unique<QTimer>(this))
    , m_statePollingTimer(std::make_unique<QTimer>(this))
{
    connect(pollingTimer.get(), &QTimer::timeout, this, &DatabaseManager::pollDatabase);
    pollingTimer->setInterval(POLLING_INTERVAL_MS);

    m_connectionTimer->setInterval(5000);  // Check every 5 seconds
    m_statePollingTimer->setInterval(200); // Poll every 200ms
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
        qDebug() << "✅ Connected to system PostgreSQL";
        enableRealTimeUpdates();  // ✅ Enable LISTEN/NOTIFY
        return true;
    }

    qDebug() << "🔄 System PostgreSQL unavailable, starting portable mode...";

    // Fall back to portable PostgreSQL
    if (startPortableMode()) {
        qDebug() << "✅ Connected to portable PostgreSQL";
        enableRealTimeUpdates();  // ✅ Enable LISTEN/NOTIFY
        return true;
    }

    // ✅ Set disconnected state and emit signal
    connected = false;
    m_isConnected = false;
    emit connectionStateChanged(connected);
    emit errorOccurred("Failed to connect to any PostgreSQL instance");
    return false;
}

bool DatabaseManager::connectToSystemPostgreSQL()
{
    try {
        // ✅ Remove existing connection if it exists
        if (QSqlDatabase::contains("system_connection")) {
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
            qDebug() << "✅ Connected to system PostgreSQL (postgres/qwerty)";
            return true;
        }
    } catch (...) {
        qDebug() << "❌ System PostgreSQL connection failed";
    }

    if (db.isOpen()) {
        db.close();
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

    // ✅ Check if server is already running before starting
    if (!isPortableServerRunning()) {
        if (!startPortablePostgreSQL()) {
            return false;
        }
        // Wait for server to start
        // QThread::sleep(1);
    } else {
        qDebug() << "✅ Portable PostgreSQL server already running";
    }

    // ✅ Remove existing connection if it exists
    if (QSqlDatabase::contains("portable_connection")) {
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

            setupDatabase();  // ✅ This will create the schema/tables
            emit connectionStateChanged(connected);
            qDebug() << "✅ Portable PostgreSQL connected with schema created";
            return true;
        }
    } catch (const std::exception& e) {
        qDebug() << "❌ Portable PostgreSQL connection failed:" << e.what();
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
        qDebug() << "❌ PostgreSQL binaries not found at:" << m_postgresPath;
        return false;
    }

    QProcess initProcess;
    QStringList arguments;
    arguments << "-D" << m_dataPath
              << "-U" << "postgres"      // ✅ CHANGED: Use postgres user
              << "-A" << "trust"         // ✅ Start with trust, convert later
              << "-E" << "UTF8";

    qDebug() << "🔧 Initializing portable database with postgres user...";
    initProcess.start(initdbPath, arguments);

    if (!initProcess.waitForFinished(100)) {
        qDebug() << "❌ Database initialization timed out";
        return false;
    }

    if (initProcess.exitCode() != 0) {
        qDebug() << "❌ Database initialization failed:" << initProcess.readAllStandardError();
        return false;
    }

    qDebug() << "✅ Portable database initialized with postgres user";
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
              << "start";  // ✅ REMOVED: -o port argument (port is in postgresql.conf)

    qDebug() << "🚀 Starting portable PostgreSQL server...";
    qDebug() << "Command:" << pgCtlPath << arguments.join(" ");

    m_postgresProcess->start(pgCtlPath, arguments);

    if (!m_postgresProcess->waitForFinished(100)) {  // ✅ Increased timeout
        qDebug() << "❌ Failed to start PostgreSQL server (timeout)";
        return false;
    }

    if (m_postgresProcess->exitCode() != 0) {
        QString errorOutput = m_postgresProcess->readAllStandardError();
        QString standardOutput = m_postgresProcess->readAllStandardOutput();
        qDebug() << "❌ PostgreSQL server start failed with exit code:" << m_postgresProcess->exitCode();
        qDebug() << "Error output:" << errorOutput;
        qDebug() << "Standard output:" << standardOutput;
        return false;
    }

    qDebug() << "✅ Portable PostgreSQL server started on port" << m_portablePort;
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

    qDebug() << "🛑 Stopping portable PostgreSQL server...";
    stopProcess.start(pgCtlPath, arguments);

    if (stopProcess.waitForFinished(5000)) {
        qDebug() << "✅ PostgreSQL server stopped successfully";
        return true;
    }

    qDebug() << "⚠️ PostgreSQL server stop timed out";
    return false;
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
    qDebug() << "🔍 Portable PostgreSQL server running check:" << isRunning;

    return isRunning;
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

    QElapsedTimer timer;
    timer.start();

    qDebug() << "🔄 SAFETY: Updating signal:" << signalId << "to aspect:" << newAspect;

    // ✅ NEW: Get current aspect for interlocking validation
    QString currentAspect = getCurrentSignalAspect(signalId);
    if (currentAspect.isEmpty()) {
        qWarning() << "❌ Could not get current aspect for signal:" << signalId;
        emit operationBlocked(signalId, "Signal not found or invalid state");
        return false;
    }

    // ✅ NEW: Interlocking validation (if service is available)
    if (m_interlockingService) {
        auto validation = m_interlockingService->validateSignalOperation(
            signalId, currentAspect, newAspect, "HMI_USER");

        if (!validation.isAllowed()) {
            qDebug() << "🚨 Signal operation blocked by interlocking:" << validation.getReason();
            emit operationBlocked(signalId, validation.getReason());
            return false;
        }

        qDebug() << "✅ Interlocking validation passed for signal" << signalId;
    } else {
        qWarning() << "⚠️ Interlocking service not available - proceeding without validation";
    }

    // ✅ EXISTING: Original database update logic
    QSqlQuery query(db);
    query.prepare("SELECT railway_control.update_signal_aspect(?, ?, 'HMI_USER')");
    query.addBindValue(signalId);
    query.addBindValue(newAspect);

    if (query.exec() && query.next()) {
        bool success = query.value(0).toBool();
        qDebug() << "✅ SAFETY: Database function returned:" << success;

        if (success) {
            // ✅ EXISTING: Verify the change actually happened
            QSqlQuery verifyQuery(db);
            verifyQuery.prepare("SELECT current_aspect_id FROM railway_control.signals WHERE signal_id = ?");
            verifyQuery.addBindValue(signalId);
            if (verifyQuery.exec() && verifyQuery.next()) {
                int currentAspectId = verifyQuery.value(0).toInt();
                qDebug() << "🔍 SAFETY: Signal" << signalId << "now has aspect_id:" << currentAspectId;
            }

            // ✅ EXISTING: Emit signals
            emit signalUpdated(signalId);
            emit signalsChanged();

            qDebug() << "✅ Signal operation completed in" << timer.elapsed() << "ms";
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

    // ✅ NEW: Get current position for interlocking validation
    QString currentPosition = getCurrentPointPosition(machineId);
    if (currentPosition.isEmpty()) {
        qWarning() << "❌ Could not get current position for point machine:" << machineId;
        emit operationBlocked(machineId, "Point machine not found or invalid state");
        return false;
    }

    // ✅ NEW: Interlocking validation (if service is available)
    if (m_interlockingService) {
        auto validation = m_interlockingService->validatePointMachineOperation(
            machineId, currentPosition, newPosition, "HMI_USER");

        if (!validation.isAllowed()) {
            qDebug() << "🚨 Point machine operation blocked by interlocking:" << validation.getReason();
            emit operationBlocked(machineId, validation.getReason());
            return false;
        }

        qDebug() << "✅ Interlocking validation passed for point machine" << machineId;
    } else {
        qWarning() << "⚠️ Interlocking service not available - proceeding without validation";
    }

    // ✅ EXISTING: Original database update logic
    QSqlQuery query(db);
    query.prepare("SELECT railway_control.update_point_position(?, ?, 'HMI_USER')");
    query.addBindValue(machineId);
    query.addBindValue(newPosition);

    if (query.exec() && query.next()) {
        bool success = query.value(0).toBool();
        if (success) {
            emit pointMachineUpdated(machineId);
            emit pointMachinesChanged();
        }
        return success;
    }

    qWarning() << "❌ SAFETY CRITICAL: Point machine update failed:" << query.lastError().text();
    return false;
}

QString DatabaseManager::getCurrentSignalAspect(const QString& signalId) {
    if (!connected) {
        qWarning() << "❌ Database not connected - cannot get signal aspect";
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
        qWarning() << "❌ Failed to get current aspect for signal" << signalId << ":" << query.lastError().text();
        return QString();
    }

    if (query.next()) {
        return query.value(0).toString();
    }

    qWarning() << "⚠️ Signal not found:" << signalId;
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

    return QString(); // Empty string indicates error
}

QStringList DatabaseManager::getProtectedTracks(const QString& signalId) {
    QSqlQuery query(db);
    query.prepare("SELECT protected_track_id FROM railway_control.signal_track_protection WHERE signal_id = ? AND is_active = TRUE");
    query.addBindValue(signalId);

    QStringList tracks;
    if (query.exec()) {
        while (query.next()) {
            tracks.append(query.value(0).toString());
        }
    }

    return tracks;
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
    qDebug() << "✅ Interlocking service connected to DatabaseManager";
}

// ✅ ADD: Database access method for interlocking branches
QSqlDatabase DatabaseManager::getDatabase() const {
    return db;
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
    if (!connected) return false;

    qDebug() << "🔧 Setting up railway control schema...";

    QSqlQuery query(db);

    // ✅ Create railway_control schema if it doesn't exist
    if (!query.exec("CREATE SCHEMA IF NOT EXISTS railway_control")) {
        qDebug() << "❌ Failed to create railway_control schema:" << query.lastError().text();
        return false;
    }

    // ✅ Create track_segments table
    QString createTrackSegments = R"(
        CREATE TABLE IF NOT EXISTS railway_control.track_segments (
            segment_id SERIAL PRIMARY KEY,
            segment_name VARCHAR(100) NOT NULL,
            start_row INTEGER,
            start_col INTEGER,
            end_row INTEGER,
            end_col INTEGER,
            track_type VARCHAR(50),
            is_occupied BOOLEAN DEFAULT FALSE,
            is_assigned BOOLEAN DEFAULT FALSE,
            occupied_by VARCHAR(100),
            is_active BOOLEAN DEFAULT TRUE,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createTrackSegments)) {
        qDebug() << "❌ Failed to create track_segments table:" << query.lastError().text();
        return false;
    }

    // ✅ Create signals table
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
        qDebug() << "❌ Failed to create signals table:" << query.lastError().text();
        return false;
    }

    // ✅ Create point_machines table
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
        qDebug() << "❌ Failed to create point_machines table:" << query.lastError().text();
        return false;
    }

    // ✅ Insert some test data
    query.exec("INSERT INTO railway_control.track_segments (segment_name, start_row, start_col, end_row, end_col, track_type) "
               "VALUES ('Track 1', 0, 0, 0, 10, 'MAIN') ON CONFLICT DO NOTHING");

    query.exec("INSERT INTO railway_control.signals (signal_name, current_aspect_id, position_row, position_col, signal_type) "
               "VALUES ('Signal A1', 1, 0, 5, 'HOME') ON CONFLICT DO NOTHING");

    qDebug() << "✅ Railway control schema and tables created successfully";
    return true;
}

void DatabaseManager::logError(const QString& operation, const QSqlError& error) {
    qWarning() << "Database error in" << operation << ":" << error.text();
}
