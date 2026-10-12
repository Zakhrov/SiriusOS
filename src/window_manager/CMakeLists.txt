project(NoirWM LANGUAGES C)

set(CMAKE_C_STANDARD 17)
find_package(PkgConfig REQUIRED)
# Title bar text.
pkg_check_modules(XFT REQUIRED IMPORTED_TARGET xft)

add_executable(NoirWM
    src/main.c src/Events.c src/Client.c src/Frame.c src/NoirWM.c src/Ewmh.c)
target_include_directories(NoirWM PRIVATE ${X11_INCLUDE_DIR} include)
target_link_libraries(NoirWM ${X11_LIBRARIES} PkgConfig::XFT NoirVulkanCompositor)

install(TARGETS NoirWM RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
