project(NoirVulkanDevice VERSION ${NOIR_VERSION} LANGUAGES C)
set(CMAKE_C_STANDARD 17)

# Vulkan device selection shared by the compositor and the shell.
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB_DRI3 REQUIRED IMPORTED_TARGET xcb xcb-dri3)

add_library(NoirVulkanDevice STATIC src/NoirVulkanDevice.c)
set_target_properties(NoirVulkanDevice PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(NoirVulkanDevice PUBLIC include)
target_link_libraries(NoirVulkanDevice PUBLIC Vulkan::Vulkan PkgConfig::XCB_DRI3)
