add_executable(libthumbnailer_test libthumbnailer_test.cpp)

qt5_use_modules(libthumbnailer_test DBus Gui Test Network)
target_link_libraries(libthumbnailer_test
    thumbnailer
    Qt5::DBus
    Qt5::Gui
    Qt5::Test
    gtest
    testutils
)
add_test(libthumbnailer libthumbnailer_test)
add_dependencies(libthumbnailer_test thumbnailer)
