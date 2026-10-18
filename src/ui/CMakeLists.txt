project(NoirUi VERSION ${NOIR_VERSION} LANGUAGES CXX)

# Noir.Ui: the design tokens, controls, icons, shared settings and Vulkan
# setup used by every Noir program (the shell, the Control Center). A static
# QML module: programs link NoirUiplugin and Q_IMPORT_QML_PLUGIN(Noir_UiPlugin).
set(CMAKE_AUTOMOC ON)

qt_add_library(NoirUi STATIC)

set(NOIR_UI_QML_FILES
    qml/Theme.qml
    qml/GlassPanel.qml
    qml/GlassReflection.qml
    qml/NoirButton.qml
    qml/NoirIcon.qml
    qml/NoirToolTip.qml
    qml/ProgressBar3D.qml
    qml/ContextMenu.qml
    qml/NoirSwitch.qml
    qml/NoirSlider.qml
    qml/NoirComboBox.qml
    qml/NoirTextField.qml
    qml/NoirScrollBar.qml
)
file(GLOB NOIR_UI_ICON_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS icons/*.svg)

# Serve the QML files from the module root (qrc:/Noir/Ui/), next to the
# generated qmldir; icons keep their icons/ prefix.
foreach(qml_file IN LISTS NOIR_UI_QML_FILES)
    get_filename_component(qml_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_name})
endforeach()
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(NoirUi
    URI Noir.Ui
    VERSION ${PROJECT_VERSION}
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Noir/Ui
    QML_FILES ${NOIR_UI_QML_FILES}
    SOURCES
        src/NoirConfig.cpp src/NoirConfig.h
        src/NoirVulkan.cpp src/NoirVulkan.h
        src/Launcher.cpp src/Launcher.h
    RESOURCES ${NOIR_UI_ICON_FILES}
)

target_include_directories(NoirUi PUBLIC src)
target_compile_definitions(NoirUi PRIVATE VK_USE_PLATFORM_XCB_KHR)
target_link_libraries(NoirUi PUBLIC
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml NoirVulkanDevice)
