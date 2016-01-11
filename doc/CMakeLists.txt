add_subdirectory(libthumbnailer-qt)

list(APPEND UNIT_TEST_TARGETS qt_example_test)
set(UNIT_TEST_TARGETS ${UNIT_TEST_TARGETS} PARENT_SCOPE)
