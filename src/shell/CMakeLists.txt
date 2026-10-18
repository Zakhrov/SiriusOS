project(NoirShell VERSION ${NOIR_VERSION} LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb)

qt_add_executable(NoirShell
    src/main.cpp
    src/X11Integration.cpp src/X11Integration.h
    src/TaskModel.cpp src/TaskModel.h
    src/AppModel.cpp src/AppModel.h
    src/SystemMonitor.cpp src/SystemMonitor.h
    src/NotificationServer.cpp src/NotificationServer.h
    src/DebugInterface.cpp src/DebugInterface.h
)

set(NOIR_QML_FILES
    qml/Main.qml
    qml/SessionShell.qml
    qml/WindowedShell.qml
    qml/Desktop.qml
    qml/DesktopIcon.qml
    qml/FloatingWidget.qml
    qml/ClockWidget.qml
    qml/SystemMonitorWidget.qml
    qml/Taskbar.qml
    qml/TaskButton.qml
    qml/SystemTray.qml
    qml/StartMenu.qml
    qml/NotificationStack.qml
    qml/NotificationCard.qml
)

# Serve every QML file from the module root, next to the generated qmldir.
# The shared components, theme and icons come from Noir.Ui (src/ui).
foreach(qml_file IN LISTS NOIR_QML_FILES)
    get_filename_component(qml_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()

qt_add_qml_module(NoirShell
    URI Noir.Shell
    VERSION ${PROJECT_VERSION}
    QML_FILES ${NOIR_QML_FILES}
)

target_include_directories(NoirShell PRIVATE src)
target_compile_definitions(NoirShell PRIVATE NOIR_VERSION="${PROJECT_VERSION}")
target_link_libraries(NoirShell PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::DBus Qt6::Svg
    PkgConfig::XCB NoirUi NoirUiplugin
)

install(TARGETS NoirShell RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
