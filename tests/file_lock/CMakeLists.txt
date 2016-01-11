add_executable(file_lock_test file_lock_test.cpp)
qt5_use_modules(file_lock_test Network)
target_link_libraries(file_lock_test Qt5::Network thumbnailer-static gtest)
add_test(file_lock file_lock_test)

add_executable(hold_lock hold_lock.cpp)
target_link_libraries(hold_lock thumbnailer-static)
list(APPEND UNIT_TEST_TARGETS hold_lock)
set(UNIT_TEST_TARGETS ${UNIT_TEST_TARGETS} PARENT_SCOPE)
