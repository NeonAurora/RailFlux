#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include "database/DatabaseManager.h"
#include "database/DatabaseInitializer.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Register C++ types with QML
    qmlRegisterType<DatabaseManager>("RailFlux.Database", 1, 0, "DatabaseManager");
    qmlRegisterType<DatabaseInitializer>("RailFlux.Database", 1, 0, "DatabaseInitializer");

    app.setWindowIcon(QIcon(":/resources/icons/railway-icon.ico"));
    qDebug() << "Icon exists??" << QFile(":/icons/railway-icon.ico").exists();


    QQmlApplicationEngine engine;

    // Create global database manager instance
    DatabaseManager* dbManager = new DatabaseManager(&app);
    engine.rootContext()->setContextProperty("globalDatabaseManager", dbManager);

    // Create global database initializer instance
    DatabaseInitializer* dbInitializer = new DatabaseInitializer(&app);
    engine.rootContext()->setContextProperty("globalDatabaseInitializer", dbInitializer);

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
