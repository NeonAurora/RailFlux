#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDriver>
#include <QTimer>
#include <QHash>
#include <QVariantMap>
#include <QVariantList>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <QProcess>
#include <QFile>
#include <QFileInfo>

class InterlockingService;

class DatabaseManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionStateChanged)

    // ✅ NEW: Data model properties for QML binding
    Q_PROPERTY(QVariantList trackSegments READ getTrackSegmentsList NOTIFY trackSegmentsChanged)
    Q_PROPERTY(QVariantList allSignals READ getAllSignalsList NOTIFY signalsChanged)
    Q_PROPERTY(QVariantList allPointMachines READ getAllPointMachinesList NOTIFY pointMachinesChanged)
    Q_PROPERTY(QVariantList textLabels READ getTextLabelsList NOTIFY textLabelsChanged)

    Q_PROPERTY(int currentPollingInterval READ getCurrentPollingInterval NOTIFY pollingIntervalChanged)
    Q_PROPERTY(QString pollingIntervalDisplay READ getPollingIntervalDisplay NOTIFY pollingIntervalChanged)

public:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

    void setInterlockingService(InterlockingService* service);

    QSqlDatabase getDatabase() const;

    Q_INVOKABLE bool connectToDatabase();
    Q_INVOKABLE bool connectToSystemPostgreSQL();
    Q_INVOKABLE void startPolling();
    Q_INVOKABLE void stopPolling();
    Q_INVOKABLE bool isConnected() const;

    Q_INVOKABLE int getCurrentPollingInterval() const;
    Q_INVOKABLE QString getPollingIntervalDisplay() const;

    // ✅ EXISTING: Component state queries
    Q_INVOKABLE QVariantMap getAllSignalStates();
    Q_INVOKABLE QVariantMap getAllTrackCircuitStates();
    Q_INVOKABLE QVariantMap getAllPointMachineStates();
    Q_INVOKABLE QString getSignalState(int signalId);
    Q_INVOKABLE bool getTrackOccupancy(int circuitId);
    Q_INVOKABLE QString getPointPosition(int machineId);

    // ✅ NEW: Complete data object queries
    Q_INVOKABLE QVariantList getTrackSegmentsList();
    Q_INVOKABLE QVariantList getAllSignalsList();
    Q_INVOKABLE QVariantList getOuterSignalsList();
    Q_INVOKABLE QVariantList getHomeSignalsList();
    Q_INVOKABLE QVariantList getStarterSignalsList();
    Q_INVOKABLE QVariantList getAdvanceStarterSignalsList();
    Q_INVOKABLE QVariantList getAllPointMachinesList();
    Q_INVOKABLE QVariantList getTextLabelsList();

    Q_INVOKABLE QStringList getProtectedTracks(const QString& signalId);
    Q_INVOKABLE QStringList getInterlockedSignals(const QString& signalId);

    // ✅ NEW: Individual object queries
    Q_INVOKABLE QVariantMap getSignalById(const QString& signalId);
    Q_INVOKABLE QVariantMap getTrackSegmentById(const QString& segmentId);
    Q_INVOKABLE QVariantMap getPointMachineById(const QString& machineId);

    // ✅ NEW: Update operations (for signal clicks, etc.)
    Q_INVOKABLE bool updateSignalAspect(const QString& signalId, const QString& newAspect);
    Q_INVOKABLE bool updatePointMachinePosition(const QString& machineId, const QString& newPosition);
    Q_INVOKABLE bool updateTrackOccupancy(const QString& trackSectionId, bool isOccupied);
    Q_INVOKABLE QVariantMap getTrackSectionById(const QString& trackSectionId);

    // ✅ NEW: Real-time notification handling
    Q_INVOKABLE void enableRealTimeUpdates();

    Q_INVOKABLE bool startPortableMode();
    Q_INVOKABLE void cleanup();

signals:
    void signalStateChanged(int signalId, const QString& newState);
    void trackCircuitStateChanged(int circuitId, bool isOccupied);
    void pointMachineStateChanged(int machineId, const QString& newPosition);
    void connectionStateChanged(bool connected);
    void dataUpdated();
    void errorOccurred(const QString& error);
    void operationBlocked(const QString& entityId, const QString& reason);
    void trackSectionUpdated(const QString& trackSectionId);
    void trackSectionsChanged();


    // ✅ NEW: Specific data change signals
    void trackSegmentsChanged();
    void signalsChanged();
    void pointMachinesChanged();
    void textLabelsChanged();
    void signalUpdated(const QString& signalId);
    void pointMachineUpdated(const QString& machineId);
    void trackSegmentUpdated(const QString& segmentId);

    void pollingIntervalChanged(int newInterval);

private slots:
    void pollDatabase();
    // ✅ FIXED: Simplified notification handler signature
    void handleDatabaseNotification(const QString& name, const QVariant& payload);

private:
    // ✅ FIXED: Added missing constant
    static constexpr int POLLING_INTERVAL_MS = 1000; // need to remove this one from code.
    static constexpr int POLLING_INTERVAL_FAST = 400000;  // production time should be 50ms
    static constexpr int POLLING_INTERVAL_SLOW = 500000; // production time should be 100ms
    InterlockingService* m_interlockingService = nullptr;

    // ✅ Database connection
    QSqlDatabase db;
    std::unique_ptr<QTimer> pollingTimer;
    bool connected;
    bool m_notificationsEnabled = false;
    bool m_notificationsWorking = false;
    QDateTime m_lastNotificationReceived;
    QTimer* m_notificationHealthTimer = nullptr;

    QString m_connectionStatus = "Not Connected";
    bool m_isConnected = false;

    QProcess* m_postgresProcess = nullptr;
    QString m_appDirectory;
    QString m_postgresPath;
    QString m_dataPath;
    int m_portablePort = 5433;
    int m_systemPort = 5432;

    // ✅ FIXED: Added missing state tracking variables
    QHash<int, QString> lastSignalStates;
    QHash<int, bool> lastTrackStates;
    QHash<int, QString> lastPointStates;

    // ✅ FIXED: Added missing private method declarations
    void detectAndEmitChanges();
    bool setupDatabase();
    void logError(const QString& operation, const QSqlError& error);
    void checkNotificationHealth();


    bool startPortablePostgreSQL();
    bool stopPortablePostgreSQL();
    bool initializePortableDatabase();
    bool isPortableServerRunning();
    QString getApplicationDirectory();

    // ✅ Row conversion helpers
    QVariantMap convertSignalRowToVariant(const QSqlQuery& query);
    QVariantMap convertTrackRowToVariant(const QSqlQuery& query);
    QVariantMap convertPointMachineRowToVariant(const QSqlQuery& query);

    QString getCurrentSignalAspect(const QString& signalId);
    QString getCurrentPointPosition(const QString& machineId);
};
