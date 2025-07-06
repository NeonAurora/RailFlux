#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "database/DatabaseManager.h"
#include "database/DatabaseInitializer.h"  // Add this

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Register C++ types with QML
    qmlRegisterType<DatabaseManager>("RailFlux.Database", 1, 0, "DatabaseManager");
    qmlRegisterType<DatabaseInitializer>("RailFlux.Database", 1, 0, "DatabaseInitializer");  // Add this

    QQmlApplicationEngine engine;

    // Create global database manager instance
    DatabaseManager* dbManager = new DatabaseManager(&app);
    engine.rootContext()->setContextProperty("globalDatabaseManager", dbManager);

    // Create global database initializer instance
    DatabaseInitializer* dbInitializer = new DatabaseInitializer(&app);  // Add this
    engine.rootContext()->setContextProperty("globalDatabaseInitializer", dbInitializer);  // Add this

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
    } else {
        qWarning() << "Failed to connect to database";
    }

    return app.exec();
}
