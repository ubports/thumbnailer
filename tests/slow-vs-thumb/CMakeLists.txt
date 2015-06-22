add_executable(slow-vs-thumb_test
    slow-vs-thumb_test.cpp
)
qt5_use_modules(slow-vs-thumb_test Network Test)
target_link_libraries(slow-vs-thumb_test
    thumbnailer
    testutils
    Qt5::Network
    Qt5::Test
    gtest)
add_test(slow-vs-thumb slow-vs-thumb_test)
