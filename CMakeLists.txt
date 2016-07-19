# Default install location. Must be set here, before setting the project.
if (NOT DEFINED CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE PATH "" FORCE)
    set(LOCAL_INSTALL "ON")
endif()

cmake_minimum_required(VERSION 3.0.2)
project(thumbnailer VERSION "2.4.0" LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)


# Suppress complaints by cmake about COMPILE_DEFINITIONS_<Config> caused by qt5 cmake macros.
cmake_policy(SET CMP0043 OLD)

string(TOLOWER "${CMAKE_BUILD_TYPE}" cmake_build_type_lower) # Build types should always be lower case

set(ACCEPTED_BUILD_TYPES "" none release debug relwithdebinfo coverage)
list(FIND ACCEPTED_BUILD_TYPES "${cmake_build_type_lower}" IS_BUILD_TYPE_ACCEPTED)
if (${IS_BUILD_TYPE_ACCEPTED} EQUAL -1)
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}\nValid types are: ${ACCEPTED_BUILD_TYPES}")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -Wall -pedantic -Wextra -fvisibility=hidden")

# Some additional warnings not included by the general flags set above.
set(EXTRA_C_WARNINGS "-Wcast-align -Wcast-qual -Wformat -Wredundant-decls -Wswitch-default")
set(EXTRA_CXX_WARNINGS "-Wnon-virtual-dtor -Wctor-dtor-privacy -Wold-style-cast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_C_WARNINGS} ${EXTRA_CXX_WARNINGS}")

# By default, for release builds, warnings become hard errors.
if ("${cmake_build_type_lower}" STREQUAL "release" OR "${cmake_build_type_lower}" STREQUAL "relwithdebinfo")
    option(Werror "Treat warnings as errors" ON)
else()
    option(Werror "Treat warnings as errors" OFF)
endif()

# If warnings are errors, don't error on deprecated declarations.
if (${Werror})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    if ("${cmake_build_type_lower}" STREQUAL "release" OR "${cmake_build_type_lower}" STREQUAL "relwithdebinfo")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=deprecated-declarations")
    endif()
endif()

add_definitions(-DQT_NO_KEYWORDS)

include(GNUInstallDirs)

# libthumbnailer-qt version
set(LIBTHUMBNAILER_QT thumbnailer-qt)
set(LIBTHUMBNAILER_QT_SO_VERSION_MAJOR "1")
set(LIBTHUMBNAILER_QT_SO_VERSION_MINOR "0")
set(LIBTHUMBNAILER_QT_SO_VERSION_MICRO "0")
set(LIBTHUMBNAILER_QT_SO_VERSION ${LIBTHUMBNAILER_QT_SO_VERSION_MAJOR}.${LIBTHUMBNAILER_QT_SO_VERSION_MINOR})
set(LIBTHUMBNAILER_QT_HDR_INSTALL_DIR ${CMAKE_INSTALL_FULL_INCLUDEDIR}/${LIBTHUMBNAILER_QT}-${LIBTHUMBNAILER_QT_SO_VERSION})

# Encoding version of cache files
set(THUMBNAILER_CACHE_VERSION "2")

# Flags for thread/address sanitizer
set(SANITIZER "" CACHE STRING "Build with -fsanitize=<value> (legal values: thread, address)")

if ("${SANITIZER}" STREQUAL "")
    # Do nothing
elseif (${SANITIZER} STREQUAL "thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fno-omit-frame-pointer -g")
elseif (${SANITIZER} STREQUAL "address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g")
else()
    message(FATAL_ERROR "Invalid SANITIZER setting: ${SANITIZER}")
endif()

# Some tests are slow, so make it possible not to run them
# during day-to-day development.
option(slowtests "Run slow tests" ON)
if (${slowtests})
    add_definitions(-DSLOW_TESTS=1)
else()
    add_definitions(-DSLOW_TESTS=0)
endif()

# Definitions for testing with valgrind.

configure_file(CTestCustom.cmake.in CTestCustom.cmake) # Tests in CTestCustom.cmake are skipped for valgrind

find_program(MEMORYCHECK_COMMAND NAMES valgrind)
if (MEMORYCHECK_COMMAND)
    set(MEMORYCHECK_COMMAND_OPTIONS
        "--suppressions=${CMAKE_SOURCE_DIR}/valgrind-suppress --errors-for-leak-kinds=definite --show-leak-kinds=definite --leak-check=full --num-callers=50 --error-exitcode=3"
    )
    add_custom_target(valgrind DEPENDS NightlyMemCheck)
else()
    message(WARNING "Cannot find valgrind: valgrind target will not be available")
endif()

include(CTest)
enable_testing()

include(EnableCoverageReport)

include(cmake/UseGSettings.cmake)

set(SHARE_PRIV_DIR ${CMAKE_INSTALL_LIBDIR}/thumbnailer)
set(SHARE_PRIV_ABS ${CMAKE_INSTALL_FULL_LIBDIR}/thumbnailer)

find_package(Boost COMPONENTS filesystem iostreams regex system REQUIRED)
find_package(Threads REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Qml REQUIRED)
find_package(Qt5Quick REQUIRED)

include(FindPkgConfig)
pkg_check_modules(GST_DEPS REQUIRED gstreamer-1.0 gstreamer-plugins-base-1.0 gstreamer-tag-1.0)
pkg_check_modules(GOBJ_DEPS REQUIRED gobject-2.0)
pkg_check_modules(GIO_DEPS REQUIRED gio-2.0 gio-unix-2.0)
pkg_check_modules(IMG_DEPS REQUIRED gdk-pixbuf-2.0 libexif)
pkg_check_modules(UNITY_API_DEPS REQUIRED libunity-api)
pkg_check_modules(APPARMOR_DEPS REQUIRED libapparmor)
pkg_check_modules(TAGLIB_DEPS REQUIRED taglib)
pkg_check_modules(CACHE_DEPS REQUIRED libpersistent-cache-cpp)

include_directories(${GST_DEPS_INCLUDE_DIRS})
include_directories(${GOBJ_DEPS_INCLUDE_DIRS})
include_directories(${GIO_DEPS_INCLUDE_DIRS})
include_directories(${IMG_DEPS_INCLUDE_DIRS})
include_directories(${UNITY_API_DEPS_INCLUDE_DIRS})
include_directories(${APPARMOR_DEPS_INCLUDE_DIRS})
include_directories(${TAGLIB_DEPS_INCLUDE_DIRS})
include_directories(include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

add_subdirectory(src)
add_subdirectory(data)
add_subdirectory(plugins/Ubuntu/Thumbnailer.0.1)
add_subdirectory(tests)
add_subdirectory(include)
add_subdirectory(man)
add_subdirectory(doc)

enable_coverage_report(
    TARGETS
        recovery_test         # Need to turn on coverage for this, to get the helper template instrumented.
        testutils
        test-seq
        thumbnailer-admin
        thumbnailer-qml
        thumbnailer-qml-static
        thumbnailer-qt
        thumbnailer-service
        thumbnailer-static
        vs-thumb
        vs-thumb-static
    FILTER
        ${CMAKE_SOURCE_DIR}/tests/*
        ${CMAKE_BINARY_DIR}/*
    TESTS
        ${UNIT_TEST_TARGETS}
)
