add_definitions(${THUMBNAILER_CFLAGS})

qt5_add_dbus_adaptor(adaptor_files dbusinterface.xml dbusinterface.h unity::thumbnailer::service::DBusInterface)
qt5_add_dbus_adaptor(adaptor_files admininterface.xml admininterface.h unity::thumbnailer::service::AdminInterface)

add_executable(thumbnailer-service
  admininterface.cpp
  dbusinterface.cpp
  handler.cpp
  inactivityhandler.cpp
  main.cpp
  ratelimiter.cpp
  stats.cpp
  ${adaptor_files}
  )

qt5_use_modules(thumbnailer-service DBus Concurrent)
target_link_libraries(thumbnailer-service thumbnailer ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(thumbnailer-service PROPERTIES AUTOMOC TRUE)

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
