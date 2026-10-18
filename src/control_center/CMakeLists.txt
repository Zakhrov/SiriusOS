project(NoirControlCenter VERSION ${NOIR_VERSION} LANGUAGES CXX)

# The Noir Control Center: system and hardware settings.
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB_EXT REQUIRED IMPORTED_TARGET xcb xcb-randr xcb-dpms xcb-xinput xcb-xkb)

qt_add_executable(NoirControlCenter
    src/main.cpp
    src/Platform.cpp src/Platform.h
    src/ControlCenterService.cpp src/ControlCenterService.h
    src/SystemInfo.cpp src/SystemInfo.h
    src/GraphicsConfig.cpp src/GraphicsConfig.h
    src/DisplayConfig.cpp src/DisplayConfig.h
    src/AudioConfig.cpp src/AudioConfig.h
    src/NetworkConfig.cpp src/NetworkConfig.h
    src/BluetoothConfig.cpp src/BluetoothConfig.h
    src/PowerConfig.cpp src/PowerConfig.h
    src/InputConfig.cpp src/InputConfig.h
    src/DateTimeConfig.cpp src/DateTimeConfig.h
    src/DefaultApps.cpp src/DefaultApps.h
)

set(NOIR_CC_QML_FILES
    qml/Main.qml
    qml/Sidebar.qml
    qml/SettingsPage.qml
    qml/Section.qml
    qml/SettingRow.qml
    qml/Banner.qml
    qml/InfoRow.qml
    qml/pages/AboutPage.qml
    qml/pages/AppearancePage.qml
    qml/pages/DisplayPage.qml
    qml/pages/SoundPage.qml
    qml/pages/NetworkPage.qml
    qml/pages/BluetoothPage.qml
    qml/pages/PowerPage.qml
    qml/pages/InputPage.qml
    qml/pages/DateTimePage.qml
    qml/pages/NotificationsPage.qml
    qml/pages/DefaultAppsPage.qml
    qml/pages/GraphicsPage.qml
)
# Serve every QML file from the module root, next to the generated qmldir.
foreach(qml_file IN LISTS NOIR_CC_QML_FILES)
    get_filename_component(qml_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()

qt_add_qml_module(NoirControlCenter
    URI Noir.ControlCenter
    VERSION ${PROJECT_VERSION}
    QML_FILES ${NOIR_CC_QML_FILES}
)

target_include_directories(NoirControlCenter PRIVATE src)
target_compile_definitions(NoirControlCenter PRIVATE NOIR_VERSION="${PROJECT_VERSION}" VK_USE_PLATFORM_XCB_KHR)
target_link_libraries(NoirControlCenter PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::DBus Qt6::Network
    PkgConfig::XCB_EXT NoirUi NoirUiplugin NoirVulkanDevice
)

install(TARGETS NoirControlCenter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES org.siriusos.NoirControlCenter.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
