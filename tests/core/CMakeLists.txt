include(FindPkgConfig)
find_package(Boost COMPONENTS system filesystem)

set(TESTLIBS core boost_filesystem boost_system leveldb gtest)

add_subdirectory(persistent_cache)
add_subdirectory(persistent_string_cache)
add_subdirectory(internal)
