add_subdirectory(core)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/settings-defaults.h
COMMAND ${CMAKE_SOURCE_DIR}/tools/parse-settings.py
-i ${CMAKE_SOURCE_DIR}/data/com.canonical.Unity.Thumbnailer.gschema.xml -o ${CMAKE_CURRENT_BINARY_DIR}/settings-defaults.h
DEPENDS ${CMAKE_SOURCE_DIR}/data/com.canonical.Unity.Thumbnailer.gschema.xml)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/settings-defaults.h
PROPERTIES HEADER_FILE_ONLY TRUE)

set_source_files_properties(settings.cpp PROPERTIES
OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/settings-defaults.h)

add_library(thumbnailer STATIC
    artdownloader.cpp
    check_access.cpp
    file_io.cpp
    image.cpp
    imageextractor.cpp
    make_directories.cpp
    safe_strerror.cpp
    settings.cpp
    trace.cpp
    thumbnailer.cpp
    ubuntuserverdownloader.cpp
    ${CMAKE_SOURCE_DIR}/include/internal/artdownloader.h
    ${CMAKE_SOURCE_DIR}/include/internal/artreply.h
    ${CMAKE_SOURCE_DIR}/include/internal/imageextractor.h
    ${CMAKE_SOURCE_DIR}/include/internal/thumbnailer.h
    ${CMAKE_SOURCE_DIR}/include/internal/ubuntuserverdownloader.h
)
qt5_use_modules(thumbnailer Core)

qt5_use_modules(thumbnailer Core Network)
set_target_properties(thumbnailer PROPERTIES AUTOMOC TRUE)

target_link_libraries(thumbnailer
    core
    ${Boost_LIBRARIES}
    ${GLIB_DEPS_LDFLAGS}
    ${GIO_DEPS_LDFLAGS}
    ${IMG_DEPS_LDFLAGS}
    ${UNITY_API_DEPS_LDFLAGS}
    ${APPARMOR_DEPS_LDFLAGS}
)

add_subdirectory(service)
add_subdirectory(thumbnailer-admin)
add_subdirectory(vs-thumb)
