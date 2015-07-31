add_executable(libthumbnailer-qt_test libthumbnailer-qt_test.cpp)

qt5_use_modules(libthumbnailer-qt_test DBus Gui Test Network)
target_link_libraries(libthumbnailer-qt_test
    thumbnailer
    Qt5::DBus
    Qt5::Gui
    Qt5::Test
    gtest
    testutils
)
add_test(libthumbnailer-qt libthumbnailer-qt_test)
add_dependencies(libthumbnailer-qt_test thumbnailer)
