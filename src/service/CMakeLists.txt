qt5_add_dbus_adaptor(adaptor_files dbusinterface.xml dbusinterface.h unity::thumbnailer::service::DBusInterface)
qt5_add_dbus_adaptor(adaptor_files admininterface.xml admininterface.h unity::thumbnailer::service::AdminInterface)

set_source_files_properties(bus.xml PROPERTIES
  CLASSNAME BusInterface)
qt5_add_dbus_interface(interface_files bus.xml businterface)

add_executable(thumbnailer-service
  admininterface.cpp
  client_config.cpp
  credentialscache.cpp
  dbusinterface.cpp
  handler.cpp
  inactivityhandler.cpp
  main.cpp
  stats.cpp
  ${adaptor_files}
  ${interface_files}
)

qt5_use_modules(thumbnailer-service DBus Concurrent)
target_link_libraries(thumbnailer-service thumbnailer-static Qt5::DBus Qt5::Concurrent)
set_target_properties(thumbnailer-service PROPERTIES AUTOMOC TRUE)
add_dependencies(thumbnailer-service vs-thumb)

install(
  TARGETS thumbnailer-service
  RUNTIME DESTINATION ${SHARE_PRIV_DIR}
)

# Install the service file.
configure_file(com.canonical.Thumbnailer.service.in com.canonical.Thumbnailer.service)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/com.canonical.Thumbnailer.service
  DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services
)
