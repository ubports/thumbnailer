add_executable(recovery_test recovery_test.cpp MockCache.cpp)
qt5_use_modules(recovery_test Network Core)
target_link_libraries(recovery_test Qt5::Core thumbnailer-static gtest gmock)
add_test(recovery recovery_test)
