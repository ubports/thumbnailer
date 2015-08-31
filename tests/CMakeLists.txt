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
    libthumbnailer-qt
    recovery
    safe_strerror
    settings
    thumbnailer
    thumbnailer-admin
    vs-thumb
)

set(slow_test_dirs
    slow-vs-thumb
    stress
)

set(UNIT_TEST_TARGETS "")

if (${slowtests})
    foreach(dir ${slow_test_dirs})
        add_subdirectory(${dir})
        list(APPEND UNIT_TEST_TARGETS "${dir}_test")
    endforeach()
    add_subdirectory(headers)
endif()

foreach(dir ${unit_test_dirs})
    add_subdirectory(${dir})
    list(APPEND UNIT_TEST_TARGETS "${dir}_test")
endforeach()

set(UNIT_TEST_TARGETS ${UNIT_TEST_TARGETS} PARENT_SCOPE)

add_subdirectory(copyright)
add_subdirectory(whitespace)

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

set_target_properties(gtest PROPERTIES COMPILE_FLAGS "-g -Wno-missing-field-initializers -Wno-old-style-cast")

if (NOT DEFINED GMOCK_ROOT)
    set(GMOCK_ROOT /usr/src/gmock)
endif()

set(GMOCK_SRC_DIR "${GMOCK_ROOT}/src")
set(GMOCK_INCLUDE_DIR ${GMOCK_ROOT})

add_library(gmock STATIC
    ${GMOCK_SRC_DIR}/gmock-all.cc
    ${GMOCK_SRC_DIR}/gmock_main.cc
)
set_target_properties(gmock PROPERTIES INCLUDE_DIRECTORIES ${GMOCK_INCLUDE_DIR})
target_link_libraries(gmock ${CMAKE_THREAD_LIBS_INIT})

set_target_properties(gmock PROPERTIES COMPILE_FLAGS "-g -Wno-missing-field-initializers")
