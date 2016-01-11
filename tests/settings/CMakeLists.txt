add_executable(settings_test settings_test.cpp)
qt5_use_modules(settings_test Network Core) # for QTemporaryDir
target_link_libraries(settings_test thumbnailer-static Qt5::Network Qt5::Core gtest)
add_test(settings settings_test)
