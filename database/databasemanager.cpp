#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>

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
    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName("localhost");
    db.setDatabaseName("railway_control_system");
    db.setUserName("postgres");
    db.setPassword("qwerty"); // TODO: Move to config file
    db.setPort(5432);

    if (!db.open()) {
        logError("Database connection", db.lastError());
        connected = false;
    } else {
        connected = true;
        qDebug() << "Database connected successfully";
        setupDatabase(); // Create tables if they don't exist
    }

    emit connectionStateChanged(connected);
    return connected;
}

bool DatabaseManager::setupDatabase() {
    QSqlQuery query(db);

    // Create signals table if not exists
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS signals (
            signal_id SERIAL PRIMARY KEY,
            signal_name VARCHAR(50) NOT NULL UNIQUE,
            signal_type VARCHAR(30) NOT NULL,
            location VARCHAR(100) NOT NULL,
            current_aspect VARCHAR(20) DEFAULT 'RED',
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )");

    // Create track_circuits table if not exists
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS track_circuits (
            circuit_id SERIAL PRIMARY KEY,
            circuit_name VARCHAR(50) NOT NULL UNIQUE,
            track_section VARCHAR(50) NOT NULL,
            is_occupied BOOLEAN DEFAULT FALSE,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )");

    // Create point_machines table if not exists
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS point_machines (
            machine_id SERIAL PRIMARY KEY,
            machine_name VARCHAR(50) NOT NULL UNIQUE,
            location VARCHAR(100) NOT NULL,
            current_position VARCHAR(20) DEFAULT 'NORMAL',
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )");

    // Insert sample data if tables are empty
    query.exec("SELECT COUNT(*) FROM signals");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("INSERT INTO signals (signal_name, signal_type, location) VALUES ('SG001', 'STARTER', 'Platform_A')");
        query.exec("INSERT INTO signals (signal_name, signal_type, location) VALUES ('SG002', 'HOME', 'Junction_Main')");
    }

    return true;
}

void DatabaseManager::startPolling() {
    if (connected) {
        pollingTimer->start();
        qDebug() << "Database polling started";
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

    detectAndEmitChanges();
    emit dataUpdated(); // Trigger QML property updates
}

void DatabaseManager::detectAndEmitChanges() {
    // Poll signals
    QSqlQuery query("SELECT signal_id, current_aspect FROM signals", db);
    while (query.next()) {
        int signalId = query.value(0).toInt();
        QString newState = query.value(1).toString();

        if (!lastSignalStates.contains(signalId) || lastSignalStates[signalId] != newState) {
            lastSignalStates[signalId] = newState;
            emit signalStateChanged(signalId, newState);
        }
    }

    // Poll track circuits
    QSqlQuery trackQuery("SELECT circuit_id, is_occupied FROM track_circuits", db);
    while (trackQuery.next()) {
        int circuitId = trackQuery.value(0).toInt();
        bool isOccupied = trackQuery.value(1).toBool();

        if (!lastTrackStates.contains(circuitId) || lastTrackStates[circuitId] != isOccupied) {
            lastTrackStates[circuitId] = isOccupied;
            emit trackCircuitStateChanged(circuitId, isOccupied);
        }
    }
}

QVariantMap DatabaseManager::getAllSignalStates() {
    QVariantMap states;
    QSqlQuery query("SELECT signal_id, current_aspect FROM signals", db);
    while (query.next()) {
        states[QString::number(query.value(0).toInt())] = query.value(1).toString();
    }
    return states;
}

QString DatabaseManager::getSignalState(int signalId) {
    QSqlQuery query(db);
    query.prepare("SELECT current_aspect FROM signals WHERE signal_id = ?");
    query.addBindValue(signalId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "RED"; // Safe default
}

bool DatabaseManager::getTrackOccupancy(int circuitId) {
    QSqlQuery query(db);
    query.prepare("SELECT is_occupied FROM track_circuits WHERE circuit_id = ?");
    query.addBindValue(circuitId);
    if (query.exec() && query.next()) {
        return query.value(0).toBool();
    }
    return false; // Safe default
}

void DatabaseManager::logError(const QString& operation, const QSqlError& error) {
    qWarning() << "Database error in" << operation << ":" << error.text();
}

QVariantMap DatabaseManager::getAllTrackCircuitStates() {
    QVariantMap states;
    QSqlQuery query("SELECT circuit_id, is_occupied FROM track_circuits", db);
    while (query.next()) {
        states[QString::number(query.value(0).toInt())] = query.value(1).toBool();
    }
    return states;
}

QVariantMap DatabaseManager::getAllPointMachineStates() {
    QVariantMap states;
    QSqlQuery query("SELECT machine_id, current_position FROM point_machines", db);
    while (query.next()) {
        states[QString::number(query.value(0).toInt())] = query.value(1).toString();
    }
    return states;
}

QString DatabaseManager::getPointPosition(int machineId) {
    QSqlQuery query(db);
    query.prepare("SELECT current_position FROM point_machines WHERE machine_id = ?");
    query.addBindValue(machineId);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return "NORMAL"; // Safe default
}
