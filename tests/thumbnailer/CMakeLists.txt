add_executable(thumbnailer_test
    thumbnailer_test.cpp
    ${interface_files}
)
qt5_use_modules(thumbnailer_test DBus Network Test)
target_link_libraries(thumbnailer_test
    thumbnailer
    testutils
    gtest)
add_dependencies(thumbnailer_test thumbnailer-service)
add_test(thumbnailer thumbnailer_test)
