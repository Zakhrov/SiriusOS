project(NoirWM LANGUAGES C)

set(CMAKE_C_STANDARD 17)
include_directories(${X11_INCLUDE_DIR} include ../compositor/include)
set(SOURCES src/main.c src/Events.c src/Client.c src/NoirWM.c src/Ewmh.c)

# X11 provides Xcomposite, Xdamage, Xrender as separate components
if(X11_Xcomposite_FOUND)
    message(STATUS "Found Xcomposite: ${X11_Xcomposite_LIB}")
endif()

if(X11_Xdamage_FOUND)
    message(STATUS "Found Xdamage: ${X11_Xdamage_LIB}")
endif()

if(X11_Xrender_FOUND)
    message(STATUS "Found Xrender: ${X11_Xrender_LIB}")
endif()

add_executable(NoirWM ${SOURCES})
target_link_libraries(NoirWM ${X11_LIBRARIES} ${X11_Xcomposite_LIB} ${X11_Xdamage_LIB} ${X11_Xrender_LIB} Vulkan::Vulkan NoirVulkanCompositor)