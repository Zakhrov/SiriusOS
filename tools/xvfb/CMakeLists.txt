project(NoirXvfbHarness LANGUAGES C)

# Input injector used by the noir-xvfb harness script.
if(X11_XTest_FOUND)
    add_executable(noir-xinject xinject.c)
    target_include_directories(noir-xinject PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(noir-xinject ${X11_LIBRARIES} ${X11_XTest_LIB})
    install(TARGETS noir-xinject RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    message(STATUS "XTest not found: noir-xinject (input injection for the Xvfb harness) will not be built")
endif()

# Boots NoirWM + NoirShell on a throwaway Xvfb display and checks that every
# shell surface maps and answers on the debug interface.
add_test(NAME noir-shell-xvfb-smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/noir-xvfb smoke)
set_tests_properties(noir-shell-xvfb-smoke PROPERTIES
    ENVIRONMENT "NOIR_BUILD_DIR=${CMAKE_BINARY_DIR};NOIR_XVFB_STATE=${CMAKE_BINARY_DIR}/xvfb-smoke"
    TIMEOUT 120)
