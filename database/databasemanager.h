#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTimer>
#include <QHash>
#include <QVariantMap>
#include <QDebug>
#include <memory>

class DatabaseManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionStateChanged)

public:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

    Q_INVOKABLE bool connectToDatabase();
    Q_INVOKABLE void startPolling();
    Q_INVOKABLE void stopPolling();
    Q_INVOKABLE bool isConnected() const;

    // Component state queries (callable from QML)
    Q_INVOKABLE QVariantMap getAllSignalStates();
    Q_INVOKABLE QVariantMap getAllTrackCircuitStates();
    Q_INVOKABLE QVariantMap getAllPointMachineStates();

    // Individual component queries
    Q_INVOKABLE QString getSignalState(int signalId);
    Q_INVOKABLE bool getTrackOccupancy(int circuitId);
    Q_INVOKABLE QString getPointPosition(int machineId);

signals:
    void signalStateChanged(int signalId, const QString& newState);
    void trackCircuitStateChanged(int circuitId, bool isOccupied);
    void pointMachineStateChanged(int machineId, const QString& newPosition);
    void connectionStateChanged(bool connected);
    void dataUpdated(); // General update signal for QML bindings

private slots:
    void pollDatabase();

private:
    QSqlDatabase db;
    std::unique_ptr<QTimer> pollingTimer;
    bool connected;

    // State caches for change detection
    QHash<int, QString> lastSignalStates;
    QHash<int, bool> lastTrackStates;
    QHash<int, QString> lastPointStates;

    static constexpr int POLLING_INTERVAL_MS = 50000;

    void detectAndEmitChanges();
    bool setupDatabase();
    void logError(const QString& operation, const QSqlError& error);
};
