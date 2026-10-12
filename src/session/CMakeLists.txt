# X session entry point: lets a display manager (or xinit) start the Noir
# Desktop Environment as a whole.
install(PROGRAMS noir-session DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES noir.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/xsessions)
