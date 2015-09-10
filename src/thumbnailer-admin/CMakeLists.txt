set(dbusinterface_xml "${CMAKE_SOURCE_DIR}/src/service/dbusinterface.xml")
set_source_files_properties(${dbusinterface_xml} PROPERTIES
    CLASSNAME ThumbnailerInterface)
qt5_add_dbus_interface(interface_files ${dbusinterface_xml} thumbnailerinterface)

set(admininterface_xml "${CMAKE_SOURCE_DIR}/src/service/admininterface.xml")
set_source_files_properties(${admininterface_xml} PROPERTIES
    CLASSNAME AdminInterface
    INCLUDE ${CMAKE_SOURCE_DIR}/src/service/stats.h)
qt5_add_dbus_interface(interface_files ${admininterface_xml} admininterface)

add_executable(thumbnailer-admin
    clear.cpp
    dbus_connection.cpp
    get_local_thumbnail.cpp
    get_remote_thumbnail.cpp
    parse_size.cpp
    show_stats.cpp
    shutdown.cpp
    thumbnailer-admin.cpp
    util.cpp
    ${CMAKE_SOURCE_DIR}/src/file_io.cpp
    ${CMAKE_SOURCE_DIR}/src/safe_strerror.cpp
    ${CMAKE_SOURCE_DIR}/src/service/stats.cpp
    ${interface_files}
)
qt5_use_modules(thumbnailer-admin DBus)
target_link_libraries(thumbnailer-admin Qt5::DBus ${CACHE_DEPS_LDFLAGS} ${Boost_LIBRARIES})
add_dependencies(thumbnailer-admin thumbnailer-service)

INSTALL(TARGETS thumbnailer-admin
        RUNTIME
        DESTINATION bin)
