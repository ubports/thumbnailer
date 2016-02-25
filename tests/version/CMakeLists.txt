add_executable(version_test
    version_test.cpp
)
qt5_use_modules(version_test Test)
target_link_libraries(version_test
    Qt5::Test
    thumbnailer-static
    testutils
    gtest)
add_test(version version_test)
