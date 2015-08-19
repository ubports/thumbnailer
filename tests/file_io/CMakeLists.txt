add_executable(file_io_test file_io_test.cpp)
qt5_use_modules(file_io_test Network)
target_link_libraries(file_io_test Qt5::Network thumbnailer-static gtest)
add_test(file_io file_io_test)
