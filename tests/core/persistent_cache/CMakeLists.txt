add_executable(persistent_cache_test persistent_cache_test.cpp)
target_link_libraries(persistent_cache_test ${TESTLIBS} core gtest)
add_definitions(-DTEST_DIR="${CMAKE_CURRENT_BINARY_DIR}")

add_test(persistent_cache persistent_cache_test)
