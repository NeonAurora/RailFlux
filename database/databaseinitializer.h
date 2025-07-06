#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariantMap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <memory>

class DatabaseInitializer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentOperation READ currentOperation NOTIFY currentOperationChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit DatabaseInitializer(QObject* parent = nullptr);
    ~DatabaseInitializer();

    // Properties
    bool isRunning() const { return m_isRunning; }
    int progress() const { return m_progress; }
    QString currentOperation() const { return m_currentOperation; }
    QString lastError() const { return m_lastError; }

    // Main operations callable from QML
    Q_INVOKABLE void resetDatabaseAsync();
    Q_INVOKABLE bool isDatabaseConnected();
    Q_INVOKABLE QVariantMap getDatabaseStatus();
    Q_INVOKABLE void testConnection();

signals:
    void isRunningChanged();
    void progressChanged();
    void currentOperationChanged();
    void lastErrorChanged();
    void resetCompleted(bool success, const QString& message);
    void connectionTestCompleted(bool success, const QString& message);

private slots:
    void performReset();

private:
    // Properties
    bool m_isRunning = false;
    int m_progress = 0;
    QString m_currentOperation;
    QString m_lastError;

    // Database connection
    QSqlDatabase db;
    QTimer* resetTimer;

    // Core operations
    bool connectToDatabase();
    bool dropExistingSchemas();
    bool createSchemas();
    bool populateConfigurationData();
    bool populateTrackSegments();
    bool populateSignals();
    bool populatePointMachines();
    bool populateTextLabels();
    bool validateDatabase();
    bool verifySchemas();

    // Advanced schema creation methods
    bool createAdvancedFunctions();
    bool createAdvancedTriggers();
    bool createGinIndexes();
    bool createViews();
    bool setupRolePermissions();

    // Helper methods
    bool executeQuery(const QString& query, const QVariantList& params = QVariantList());
    bool executeSchemaScript();
    void setError(const QString& error);
    void updateProgress(int value, const QString& operation);

    // Data population helpers
    int insertSignalType(const QString& typeCode, const QString& typeName, int maxAspects);
    int insertSignalAspect(const QString& aspectCode, const QString& aspectName, const QString& colorCode, int safetyLevel);
    int insertPointPosition(const QString& positionCode, const QString& positionName);

    // StationData.js conversion functions
    QJsonArray getTrackSegmentsData();
    QJsonArray getOuterSignalsData();
    QJsonArray getHomeSignalsData();
    QJsonArray getStarterSignalsData();
    QJsonArray getAdvancedStarterSignalsData();
    QJsonArray getPointMachinesData();
    QJsonArray getTextLabelsData();
};
