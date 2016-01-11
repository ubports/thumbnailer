add_executable(image_test image_test.cpp)
qt5_use_modules(image_test Network)
target_link_libraries(image_test thumbnailer-static Qt5::Network gtest gtest_main)
add_test(image image_test)
