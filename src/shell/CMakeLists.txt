project(NoirShell LANGUAGES CXX)

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
)

set(NOIR_QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/SessionShell.qml
    qml/WindowedShell.qml
    qml/GlassPanel.qml
    qml/NoirButton.qml
    qml/NoirToolTip.qml
    qml/NoirIcon.qml
    qml/Desktop.qml
    qml/DesktopIcon.qml
    qml/FloatingWidget.qml
    qml/ClockWidget.qml
    qml/SystemMonitorWidget.qml
    qml/Taskbar.qml
    qml/TaskButton.qml
    qml/SystemTray.qml
    qml/StartMenu.qml
    qml/ContextMenu.qml
    qml/NotificationStack.qml
    qml/NotificationCard.qml
    qml/ProgressBar3D.qml
)
set(NOIR_ICON_FILES
    icons/noir-start.svg
    icons/noir-app.svg
    icons/noir-terminal.svg
    icons/noir-folder.svg
    icons/noir-settings.svg
    icons/noir-trash.svg
    icons/noir-network.svg
    icons/noir-volume.svg
    icons/noir-battery.svg
    icons/noir-bell.svg
    icons/noir-search.svg
    icons/noir-power.svg
)

# Serve every QML file from the module root so the generated qmldir (and the
# Theme singleton it declares) is picked up by the implicit directory import.
foreach(qml_file IN LISTS NOIR_QML_FILES)
    get_filename_component(qml_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(NoirShell
    URI Noir.Shell
    VERSION 1.0
    QML_FILES ${NOIR_QML_FILES}
    RESOURCES ${NOIR_ICON_FILES}
)

target_include_directories(NoirShell PRIVATE src)
target_link_libraries(NoirShell PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::DBus Qt6::Svg
    PkgConfig::XCB
)

install(TARGETS NoirShell RUNTIME DESTINATION bin)
