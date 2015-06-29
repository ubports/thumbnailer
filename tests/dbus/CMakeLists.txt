add_executable(dbus_test dbus_test.cpp)
qt5_use_modules(dbus_test Network DBus Test)
target_link_libraries(dbus_test
    thumbnailer
    testutils
    Qt5::Network
    Qt5::DBus
    Qt5::Test
    gtest
)
add_test(dbus dbus_test)
add_dependencies(dbus_test thumbnailer-service)
