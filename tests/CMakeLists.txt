set(TESTDATADIR ${CMAKE_CURRENT_SOURCE_DIR}/media)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

configure_file(testsetup.h.in testsetup.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(utils)

set(unit_test_dirs
    check_access
    dbus
    download
    file_io
    gobj_ptr
    image
    qml
    libthumbnailer
    safe_strerror
    settings
    thumbnailer
    thumbnailer-admin
    vs-thumb
)

set(UNIT_TEST_TARGETS "")

if (${slowtests})
    add_subdirectory(slow-vs-thumb)
    list(APPEND UNIT_TEST_TARGETS slow-vs-thumb_test)
    add_subdirectory(headers)
endif()

foreach(dir ${unit_test_dirs})
    add_subdirectory(${dir})
    list(APPEND UNIT_TEST_TARGETS "${dir}_test")
endforeach()

set(UNIT_TEST_TARGETS ${UNIT_TEST_TARGETS} PARENT_SCOPE)

add_subdirectory(copyright)
add_subdirectory(whitespace)

add_subdirectory(core)

if (NOT DEFINED GTEST_ROOT)
    set(GTEST_ROOT /usr/src/gtest)
endif()

set(GTEST_SRC_DIR "${GTEST_ROOT}/src")
set(GTEST_INCLUDE_DIR ${GTEST_ROOT})

add_library(gtest STATIC
    ${GTEST_SRC_DIR}/gtest-all.cc
    ${GTEST_SRC_DIR}/gtest_main.cc
)
set_target_properties(gtest PROPERTIES INCLUDE_DIRECTORIES ${GTEST_INCLUDE_DIR})
target_link_libraries(gtest ${CMAKE_THREAD_LIBS_INIT})

# Make sure we always build gtest with debug info, otherwise we don't
# get line numbers for the code that is dragged in by the test macros.
set_property(TARGET gtest APPEND_STRING PROPERTY COMPILE_FLAGS " -g")

# gtest won't compile cleanly without this.
set_property(TARGET gtest APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-missing-field-initializers -Wno-old-style-cast")
