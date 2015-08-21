include_directories(${CMAKE_BINARY_DIR}/tests)
add_executable(qt_example qt_example.cpp)
qt5_use_modules(qt_example Core Gui Test)
target_link_libraries(qt_example
    Qt5::Core
    Qt5::Gui
    Qt5::Test
    thumbnailer-qt
    gtest
)
set_target_properties(qt_example PROPERTIES AUTOMOC TRUE)
add_dependencies(qt_example thumbnailer-service)

add_test(example_test qt_example)
