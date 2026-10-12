project(NoirVulkanCompositor LANGUAGES C)
set(CMAKE_C_STANDARD 17)

# Shaders are compiled to SPIR-V at build time and embedded as C arrays.
find_program(GLSLANG_VALIDATOR NAMES glslangValidator glslang REQUIRED)
set(SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_DIR})
set(SHADER_HEADERS)
foreach(shader quad.vert quad.frag)
    string(REPLACE "." "_" shader_name ${shader})
    set(header ${SHADER_DIR}/${shader_name}.h)
    add_custom_command(
        OUTPUT ${header}
        COMMAND ${GLSLANG_VALIDATOR} -V --quiet --vn ${shader_name}_spv
                -o ${header} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}
        DEPENDS shaders/${shader}
        COMMENT "Compiling ${shader} to SPIR-V")
    list(APPEND SHADER_HEADERS ${header})
endforeach()

add_library(NoirVulkanCompositor SHARED
    src/VulkanRenderer.c src/Compositor.c ${SHADER_HEADERS})
target_include_directories(NoirVulkanCompositor
    PUBLIC include
    PRIVATE ${X11_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(NoirVulkanCompositor
    ${X11_LIBRARIES} ${X11_Xext_LIB} ${X11_Xcomposite_LIB} ${X11_Xdamage_LIB} ${X11_Xfixes_LIB}
    Vulkan::Vulkan)

install(TARGETS NoirVulkanCompositor LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
