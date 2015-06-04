add_executable(download_test download_test.cpp)
qt5_use_modules(download_test Core Network Test)
target_link_libraries(download_test thumbnailer testutils gtest)
add_test(download download_test)
