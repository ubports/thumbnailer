add_schema(com.canonical.Unity.Thumbnailer.gschema.xml)

# compile schema - only needed for tests; actual schema is handled as part of add_schema above
execute_process (COMMAND ${PKG_CONFIG_EXECUTABLE} gio-2.0 --variable glib_compile_schemas OUTPUT_VARIABLE _glib_comple_schemas OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process (COMMAND ${_glib_comple_schemas} ${CMAKE_CURRENT_SOURCE_DIR} --targetdir=${CMAKE_CURRENT_BINARY_DIR} OUTPUT_STRIP_TRAILING_WHITESPACE)

install(
  FILES
    album_missing.png
    video_missing.png
  DESTINATION
    ${CMAKE_INSTALL_DATADIR}/thumbnailer/icons
)

configure_file(
  thumbnailer.in
  thumbnailer
  @ONLY
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/thumbnailer"
  DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/apport/blacklist.d"
)
