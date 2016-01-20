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

message("Arch: ${CMAKE_LIBRARY_ARCHITECTURE}")
if (${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "powerpc" OR ${CMAKE_LIBRARY_ARCHITECTURE} STREQUAL "s390x")
    message("Skipping qml test for ${CMAKE_LIBRARY_ARCHITECTURE}")
else()
    add_test(qml ${CMAKE_SOURCE_DIR}/tools/run-xvfb.sh ./qml_test -import ${CMAKE_BINARY_DIR}/plugins)
endif()
