#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include "database/DatabaseManager.h"
#include "database/DatabaseInitializer.h"
#include "interlocking/InterlockingService.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Register C++ types with QML
    qmlRegisterType<DatabaseManager>("RailFlux.Database", 1, 0, "DatabaseManager");
    qmlRegisterType<DatabaseInitializer>("RailFlux.Database", 1, 0, "DatabaseInitializer");
    qmlRegisterType<InterlockingService>("RailFlux.Interlocking", 1, 0, "InterlockingService");  // NEW
    qmlRegisterType<ValidationResult>("RailFlux.Interlocking", 1, 0, "ValidationResult");        // NEW

    app.setWindowIcon(QIcon(":/resources/icons/railway-icon.ico"));
    qDebug() << "Icon exists??" << QFile(":/icons/railway-icon.ico").exists();

    QQmlApplicationEngine engine;

    // Create global instances
    DatabaseManager* dbManager = new DatabaseManager(&app);
    DatabaseInitializer* dbInitializer = new DatabaseInitializer(&app);
    InterlockingService* interlockingService = new InterlockingService(dbManager, &app);  // NEW

    engine.rootContext()->setContextProperty("globalDatabaseManager", dbManager);
    engine.rootContext()->setContextProperty("globalDatabaseInitializer", dbInitializer);
    engine.rootContext()->setContextProperty("globalInterlockingService", interlockingService);  // NEW

    dbManager->setInterlockingService(interlockingService);

    QObject::connect(dbManager, &DatabaseManager::connectionStateChanged,
                     [interlockingService](bool connected) {
                         if (connected) {
                             interlockingService->initialize();
                         }
                     });

    // ✅ NEW: Connect freeze signal for safety system monitoring
    QObject::connect(interlockingService, &InterlockingService::systemFreezeRequired,
                     [](const QString& trackSegmentId, const QString& reason, const QString& details) {
                         qCritical() << "🚨🚨🚨 FREEZE SIGNAL DETECTED IN MAIN.CPP 🚨🚨🚨";
                         qCritical() << "🚨 SYSTEM FREEZE ACTIVATED 🚨";
                         qCritical() << "Track Segment SEGMENT ID:" << trackSegmentId;
                         qCritical() << "Reason:" << reason;
                         qCritical() << "Details:" << details;
                         qCritical() << "Timestamp:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
                         qCritical() << "🚨 MANUAL INTERVENTION REQUIRED 🚨";
                         qCritical() << "🚨🚨🚨 END FREEZE SIGNAL 🚨🚨🚨";
                     });

    // ✅ ADD: Cleanup on application exit
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [dbManager]() {
        qDebug() << "🧹 Application shutting down, cleaning up database...";
        dbManager->cleanup();
        dbManager->stopPolling();
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("RailFlux", "Main");

    // Start database connection and polling
    if (dbManager->connectToDatabase()) {
        dbManager->startPolling();
        dbManager->enableRealTimeUpdates();  // ✅ Enable LISTEN/NOTIFY
    } else {
        qWarning() << "Failed to connect to database";
    }

    return app.exec();
}
