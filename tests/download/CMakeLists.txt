add_executable(download_test download_test.cpp)
qt5_use_modules(download_test Core Network Test)
target_link_libraries(download_test
    thumbnailer
    testutils
    Qt5::Network
    Qt5::Core
    Qt5::Test
    gtest
)
add_test(download download_test)
