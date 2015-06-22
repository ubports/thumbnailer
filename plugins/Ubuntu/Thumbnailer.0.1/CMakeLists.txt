
set(QML_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/qt5/qml/Ubuntu/Thumbnailer.0.1")

set(dbusinterface_xml "${CMAKE_SOURCE_DIR}/src/service/dbusinterface.xml")
set_source_files_properties(${dbusinterface_xml} PROPERTIES
  CLASSNAME ThumbnailerInterface)
qt5_add_dbus_interface(interface_files ${dbusinterface_xml} thumbnailerinterface)

add_library(thumbnailer-qml MODULE
  albumartgenerator.cpp
  artgeneratorcommon.cpp
  artistartgenerator.cpp
  plugin.cpp
  thumbnailerimageresponse.cpp
  thumbnailgenerator.cpp
  ${interface_files}
)

set_target_properties(thumbnailer-qml PROPERTIES AUTOMOC TRUE)
qt5_use_modules(thumbnailer-qml Qml Quick DBus)
target_link_libraries(thumbnailer-qml Qt5::Qml Qt5::Quick Qt5::DBus)

configure_file(qmldir qmldir)

install(
  TARGETS thumbnailer-qml
  LIBRARY DESTINATION ${QML_PLUGIN_DIR}
)

install(
  FILES qmldir
  DESTINATION ${QML_PLUGIN_DIR}
)
