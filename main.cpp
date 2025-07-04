#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "database/DatabaseManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Register C++ types with QML
    qmlRegisterType<DatabaseManager>("RailFlux.Database", 1, 0, "DatabaseManager");

    QQmlApplicationEngine engine;

    // Create global database manager instance
    DatabaseManager* dbManager = new DatabaseManager(&app);
    engine.rootContext()->setContextProperty("globalDatabaseManager", dbManager);

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
