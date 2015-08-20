include_directories(${CMAKE_SOURCE_DIR}/tests)
include_directories(${CMAKE_BINARY_DIR}/tests)

add_executable(thumbnailer-admin_test
    thumbnailer-admin_test.cpp
)
qt5_use_modules(thumbnailer-admin_test Network Test)
target_link_libraries(thumbnailer-admin_test
    gtest
    thumbnailer-static
    testutils
    Qt5::Network
    Qt5::Test
)
add_dependencies(thumbnailer-admin_test thumbnailer-admin thumbnailer-service)
add_test(thumbnailer-admin thumbnailer-admin_test)
