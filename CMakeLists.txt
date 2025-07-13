cmake_minimum_required(VERSION 3.16)

project(RailFlux VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)


find_package(Qt6 REQUIRED COMPONENTS Quick Sql)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appRailFlux
    main.cpp
)

qt_add_qml_module(appRailFlux
    URI RailFlux
    VERSION 1.0

    QML_FILES
        Main.qml
        components/TrackSegment.qml
        components/LevelCrossingGate.qml
        components/GridCanvas.qml
        layouts/StationLayout.qml
        data/StationData.js
        components/OuterSignal.qml
        components/HomeSignal.qml
        components/StarterSignal.qml
        components/PointMachine.qml
        components/AdvanceStarterSignal.qml

    SOURCES
    database/DatabaseManager.h
        database/DatabaseManager.cpp
        database/DatabaseInitializer.h
        database/DatabaseInitializer.cpp

    RESOURCES
        sql/sql_coomands_railflux.sql
        SOURCES
        SOURCES
        SOURCES interlocking/InterlockingService.h interlocking/InterlockingService.cpp
        SOURCES interlocking/SignalBranch.h interlocking/SignalBranch.cpp
        SOURCES interlocking/TrackCircuitBranch.h interlocking/TrackCircuitBranch.cpp
        SOURCES interlocking/PointMachineBranch.h interlocking/PointMachineBranch.cpp
)

qt_add_resources(appRailFlux "app_resources"
    PREFIX "/"
    FILES
        resources/icons/railway-icon.ico
)

set_target_properties(appRailFlux PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appRailFlux
    PRIVATE Qt6::Quick Qt6::Sql
)
