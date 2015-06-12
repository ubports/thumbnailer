include(FindPkgConfig)
find_package(Boost COMPONENTS system filesystem)

set(TESTLIBS core boost_filesystem boost_system leveldb gtest)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g --coverage ")

add_subdirectory(persistent_cache)
add_subdirectory(persistent_string_cache)
add_subdirectory(internal)
