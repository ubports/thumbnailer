add_executable(vs-thumb
  vs-thumb.cpp
  thumbnailextractor.cpp
)

target_link_libraries(vs-thumb ${GST_DEPS_LDFLAGS} ${IMG_DEPS_LDFLAGS} ${GIO_DEPS_LDFLAGS})

install(
  TARGETS vs-thumb
  ARCHIVE DESTINATION ${SHARE_PRIV_DIR}
  RUNTIME DESTINATION ${SHARE_PRIV_DIR}
  LIBRARY DESTINATION ${SHARE_PRIV_DIR}
)
