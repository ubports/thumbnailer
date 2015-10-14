add_executable(vs-thumb_test
  vs-thumb_test.cpp
)
qt5_use_modules(vs-thumb_test Core Network Test)
target_link_libraries(vs-thumb_test
  vs-thumb-static
  thumbnailer-static
  ${GST_DEPS_LDFLAGS}
  ${IMG_DEPS_LDFLAGS}
  ${GIO_DEPS_LDFLAGS}
  Qt5::Test
  testutils
  gtest
)
add_test(vs-thumb vs-thumb_test)
add_dependencies(vs-thumb_test vs-thumb)
