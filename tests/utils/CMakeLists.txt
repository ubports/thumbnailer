pkg_check_modules(QTDBUSTEST_DEPS REQUIRED libqtdbustest-1)
add_definitions(${QTDBUSTEST_DEPS_CFLAGS})

set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/src/service/dbusinterface.xml" PROPERTIES
    CLASSNAME ThumbnailerInterface
)
qt5_add_dbus_interface(
    interface_files
    "${CMAKE_SOURCE_DIR}/src/service/dbusinterface.xml"
    thumbnailerinterface
)

set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/src/service/admininterface.xml" PROPERTIES
    CLASSNAME AdminInterface
    INCLUDE ${CMAKE_SOURCE_DIR}/src/service/stats.h
)
qt5_add_dbus_interface(
    interface_files
    "${CMAKE_SOURCE_DIR}/src/service/admininterface.xml"
    admininterface
)

add_library(testutils STATIC
    artserver.cpp
    dbusserver.cpp
    supports_decoder.cpp
    testutils.cpp
    ${interface_files}
    ${CMAKE_SOURCE_DIR}/src/service/stats.cpp
)
qt5_use_modules(testutils Core DBus Test)
target_link_libraries(testutils
    Qt5::DBus Qt5::Core Qt5::Test ${QTDBUSTEST_DEPS_LIBRARIES} thumbnailer-static
)
