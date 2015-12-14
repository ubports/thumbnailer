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
add_test(qml ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh ${CMAKE_BINARY_DIR}/stderr.log ${CMAKE_BINARY_DIR}/plugins)
