add_executable(settings_test settings_test.cpp)
target_link_libraries(settings_test thumbnailer gtest)
qt5_use_modules(settings_test Network Core) # for QTemporaryDir
add_test(settings settings_test)
