add_subdirectory(core)

add_library(thumbnailer STATIC
    artdownloader.cpp
    file_io.cpp
    image.cpp
    imageextractor.cpp
    make_directories.cpp
    safe_strerror.cpp
    settings.cpp
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
)

add_subdirectory(service)
add_subdirectory(thumbnailer-admin)
add_subdirectory(vs-thumb)
