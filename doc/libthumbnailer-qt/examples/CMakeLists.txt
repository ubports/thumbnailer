include_directories(${CMAKE_BINARY_DIR}/tests ${CMAKE_SOURCE_DIR}/tests)
add_executable(qt_example_test qt_example_test.cpp)
qt5_use_modules(qt_example_test Core Gui Test)
target_link_libraries(qt_example_test
    gtest
    Qt5::Core
    Qt5::Gui
    Qt5::Test
    testutils
    thumbnailer-qt
)
set_target_properties(qt_example_test PROPERTIES AUTOMOC TRUE)
add_dependencies(qt_example_test thumbnailer-service)

add_test(qt_example qt_example_test)

list(APPEND UNIT_TEST_TARGETS qt_example_test)
set(UNIT_TEST_TARGETS ${UNIT_TEST_TARGETS} PARENT_SCOPE)
