add_executable(vsthumb_test
  vsthumb_test.cpp
  ${CMAKE_SOURCE_DIR}/src/vs-thumb/thumbnailextractor.cpp)
target_link_libraries(vsthumb_test
  ${GST_DEPS_LDFLAGS}
  ${IMG_DEPS_LDFLAGS}
  ${GIO_DEPS_LDFLAGS}
  gtest)
add_test(vsthumb vsthumb_test)
