set(section1
    thumbnailer-admin.1
    thumbnailer-service.1
)

set(section5
    thumbnailer-settings.5
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/${section1}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/man/man1
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/${section5}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/man/man5
)
