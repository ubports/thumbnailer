add_executable(gobj_ptr_test gobj_ptr_test.cpp)
target_link_libraries(gobj_ptr_test ${GOBJ_DEPS_LDFLAGS} gtest)
add_test(gobj_ptr gobj_ptr_test)
