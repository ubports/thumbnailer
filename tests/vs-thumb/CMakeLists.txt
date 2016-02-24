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
add_dependencies(vs-thumb_test vs-thumb)

execute_process(COMMAND lsb_release -c -s OUTPUT_VARIABLE distro OUTPUT_STRIP_TRAILING_WHITESPACE)
if ("${distro}" STREQUAL "vivid" AND "${CMAKE_LIBRARY_ARCHITECTURE}" STREQUAL "powerpc64le-linux-gnu")
    message("skipping vs-thumb test for ${CMAKE_LIBRARY_ARCHITECTURE}")
else()
    add_test(vs-thumb vs-thumb_test)
endif()
