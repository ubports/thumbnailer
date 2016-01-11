add_executable(libthumbnailer-qt_test libthumbnailer-qt_test.cpp)
set_target_properties(libthumbnailer-qt_test PROPERTIES AUTOMOC TRUE)

qt5_use_modules(libthumbnailer-qt_test DBus Gui Test Network)
target_link_libraries(libthumbnailer-qt_test
    thumbnailer-qt
    Qt5::DBus
    Qt5::Gui
    Qt5::Test
    gtest
    testutils
    ${GST_DEPS_LDFLAGS}
)
add_test(libthumbnailer-qt libthumbnailer-qt_test)
add_dependencies(libthumbnailer-qt_test thumbnailer-qt)
list(APPEND UNIT_TEST_TARGETS "libthumbnailer-qt_test")

set(UNIT_TEST_TARGETS ${UNIT_TEST_TARGETS} PARENT_SCOPE)
