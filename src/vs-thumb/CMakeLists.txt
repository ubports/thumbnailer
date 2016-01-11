add_library(vs-thumb-static STATIC
    thumbnailextractor.cpp
)
qt5_use_modules(vs-thumb-static Core)

add_executable(vs-thumb
    vs-thumb.cpp
)
target_link_libraries(vs-thumb
    vs-thumb-static
    thumbnailer-static
    ${GST_DEPS_LDFLAGS}
    ${IMG_DEPS_LDFLAGS}
    ${GIO_DEPS_LDFLAGS})

install(
    TARGETS vs-thumb
    ARCHIVE DESTINATION ${SHARE_PRIV_DIR}
    RUNTIME DESTINATION ${SHARE_PRIV_DIR}
    LIBRARY DESTINATION ${SHARE_PRIV_DIR}
)

add_executable(test-seq
    test-seq.cpp
)
target_link_libraries(test-seq
    vs-thumb-static
    thumbnailer-static
    ${GST_DEPS_LDFLAGS}
    ${IMG_DEPS_LDFLAGS}
    ${GIO_DEPS_LDFLAGS}
)
