add_executable(vs-thumb_test
  vs-thumb_test.cpp
  ${CMAKE_SOURCE_DIR}/src/vs-thumb/thumbnailextractor.cpp)
target_link_libraries(vs-thumb_test
  ${GST_DEPS_LDFLAGS}
  ${IMG_DEPS_LDFLAGS}
  ${GIO_DEPS_LDFLAGS}
  testutils
  gtest)
add_test(vs-thumb vs-thumb_test)
