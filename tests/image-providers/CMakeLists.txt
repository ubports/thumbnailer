include_directories(${CMAKE_SOURCE_DIR}/plugins/Ubuntu/Thumbnailer.0.1)

add_executable(image-providers_test provider_test.cpp)
qt5_use_modules(image-providers_test Quick Test)
target_link_libraries(image-providers_test
  thumbnailer-qml-static
  testutils
  Qt5::Quick
  Qt5::Test
  gtest)
add_test(image-providers image-providers_test)
