add_executable(qml_test qml_test.cpp)
qt5_use_modules(qml_test Qml DBus QuickTest Test)
target_link_libraries(qml_test
    testutils
    Qt5::Qml
    Qt5::DBus
    Qt5::QuickTest
    Qt5::Test
)
add_dependencies(qml_test thumbnailer-service thumbnailer-qml)
add_test(qml xvfb-run -a -s "-screen 0 800x600x24" ./qml_test -import ${CMAKE_BINARY_DIR}/plugins)
