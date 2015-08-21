set(dbusinterface_xml "${CMAKE_SOURCE_DIR}/src/service/dbusinterface.xml")
set_source_files_properties(${dbusinterface_xml} PROPERTIES
  CLASSNAME ThumbnailerInterface)
qt5_add_dbus_interface(interface_files ${dbusinterface_xml} thumbnailerinterface)

add_library(${LIBTHUMBNAILER_QT} SHARED
  libthumbnailer-qt.cpp
  ${CMAKE_SOURCE_DIR}/include/unity/thumbnailer/qt/thumbnailer-qt.h
  ${interface_files}
  ${CMAKE_SOURCE_DIR}/src/artgeneratorcommon.cpp
)

set_target_properties(${LIBTHUMBNAILER_QT} PROPERTIES
    VERSION "${LIBTHUMBNAILER_QT_SO_VERSION_MAJOR}.${LIBTHUMBNAILER_QT_SO_VERSION_MINOR}.${LIBTHUMBNAILER_QT_SO_VERSION_MICRO}"
    SOVERSION ${LIBTHUMBNAILER_QT_SO_VERSION}
)

qt5_use_modules(${LIBTHUMBNAILER_QT} DBus Gui)
target_link_libraries(${LIBTHUMBNAILER_QT} thumbnailer-static Qt5::DBus Qt5::Gui)
set_target_properties(${LIBTHUMBNAILER_QT} PROPERTIES AUTOMOC TRUE)
add_dependencies(${LIBTHUMBNAILER_QT} thumbnailer-service)

install(
  TARGETS thumbnailer-qt
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
)

# Set up package config.
configure_file(lib${LIBTHUMBNAILER_QT}.pc.in lib${LIBTHUMBNAILER_QT}.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lib${LIBTHUMBNAILER_QT}.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig)
