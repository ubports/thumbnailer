add_executable(art_extractor_test
    art_extractor_test.cpp
)
qt5_use_modules(art_extractor_test Test)
target_link_libraries(art_extractor_test
    Qt5::Test
    thumbnailer-static
    testutils
    gtest)
add_test(art_extractor art_extractor_test)
