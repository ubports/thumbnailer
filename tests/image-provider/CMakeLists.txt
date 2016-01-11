include_directories(${CMAKE_SOURCE_DIR}/plugins/Ubuntu/Thumbnailer.0.1)

add_executable(image-provider_test image-provider_test.cpp)
qt5_use_modules(image-provider_test Quick Test)
target_link_libraries(image-provider_test
  thumbnailer-qml-static
  testutils
  Qt5::Quick
  Qt5::Test
  gtest)
add_test(image-provider ${CMAKE_SOURCE_DIR}/tools/run-xvfb.sh ./image-provider_test)
