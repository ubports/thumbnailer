set(dbusinterface_xml "${CMAKE_SOURCE_DIR}/src/service/dbusinterface.xml")
set_source_files_properties(${dbusinterface_xml} PROPERTIES
  CLASSNAME ThumbnailerInterface)
qt5_add_dbus_interface(interface_files ${dbusinterface_xml} thumbnailerinterface)

set(SO_VERSION_MAJOR "1")
set(SO_VERSION_MINOR "0")
set(SO_VERSION_MICRO "0")

set(SO_VERSION ${SO_VERSION_MAJOR}.${SO_VERSION_MINOR})


add_library(thumbnailer-qt SHARED
  libthumbnailer-qt.cpp
  ${CMAKE_SOURCE_DIR}/include/libthumbnailer-qt/thumbnailer-qt.h
  ${interface_files}
  ${CMAKE_SOURCE_DIR}/src/artgeneratorcommon.cpp
)

set_target_properties(thumbnailer-qt PROPERTIES
    VERSION "${SO_VERSION_MAJOR}.${SO_VERSION_MINOR}.${SO_VERSION_MICRO}"
    SOVERSION ${SO_VERSION}
)

set(symbol_map "${CMAKE_SOURCE_DIR}/src/libthumbnailer-qt/libthumbnailer-qt.map")
set_target_properties(thumbnailer-qt PROPERTIES
                      LINK_FLAGS "${ldflags} -Wl,--version-script,${symbol_map} ")
set_target_properties(thumbnailer-qt PROPERTIES LINK_DEPENDS ${symbol_map})

qt5_use_modules(thumbnailer-qt DBus Gui)
target_link_libraries(thumbnailer-qt thumbnailer-static Qt5::DBus Qt5::Gui)
set_target_properties(thumbnailer-qt PROPERTIES AUTOMOC TRUE)
add_dependencies(thumbnailer-qt thumbnailer-service)

install(
  TARGETS thumbnailer-qt
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
