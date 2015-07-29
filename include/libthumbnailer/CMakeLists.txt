install(FILES thumbnailer.h DESTINATION include)

# Build and install a pkg-config file
set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${prefix}/bin)
set(libdir ${prefix}/${LIBDIR})
set(includedir ${prefix}/include)
set(pkg-name "lib${COL_LIB_BASENAME}")
configure_file(thumbnailer.pc.in thumbnailer.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/thumbnailer.pc DESTINATION ${LIBDIR}/pkgconfig)