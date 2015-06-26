add_executable(stress_test stress_test.cpp)
qt5_use_modules(stress_test Network DBus Test)
target_link_libraries(stress_test
    thumbnailer
    testutils
    Qt5::Network
    Qt5::DBus
    Qt5::Test
    gtest
)
add_test(stress stress_test)
add_dependencies(stress_test thumbnailer-service)
