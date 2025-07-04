cmake_minimum_required(VERSION 3.16)

project(RailFlux VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
        components/BasicSignal.qml
        components/LevelCrossingGate.qml
        components/GridCanvas.qml
        layouts/StationLayout.qml
        data/StationData.js

    SOURCES
        database/databasemanager.h
        database/databasemanager.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appRailFlux PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appRailFlux
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appRailFlux
    PRIVATE Qt6::Quick Qt6::Sql
)

include(GNUInstallDirs)
install(TARGETS appRailFlux
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
