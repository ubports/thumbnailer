# We add -g so we get debug info for the gtest stack frames with gdb.
# The warnings are suppressed so we get a noise-free build for gtest and gmock.

set(old_cxx_flags ${CMAKE_CXX_FLAGS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wno-old-style-cast -Wno-missing-field-initializers")
find_package(GMock)
set(CMAKE_CXX_FLAGS ${old_cxx_flags})

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
    file_lock
    gobj_ptr
    image
    image-provider
    qml
    libthumbnailer-qt
    recovery
    safe_strerror
    settings
    thumbnailer
    thumbnailer-admin
    vs-thumb
)

set(slow_test_dirs
    backoff_adjuster
    slow-vs-thumb
    stress
)

set(UNIT_TEST_TARGETS "")

foreach(dir ${unit_test_dirs})
    add_subdirectory(${dir})
    list(APPEND UNIT_TEST_TARGETS "${dir}_test")
endforeach()

add_subdirectory(copyright)
add_subdirectory(whitespace)

if (${slowtests})
    foreach(dir ${slow_test_dirs})
        add_subdirectory(${dir})
        list(APPEND UNIT_TEST_TARGETS "${dir}_test")
    endforeach()
    add_subdirectory(headers)
endif()

set(UNIT_TEST_TARGETS ${UNIT_TEST_TARGETS} PARENT_SCOPE)
