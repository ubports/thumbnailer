#
# Documentation
#
# Pass -DDEVEL_DOCS=ON to cmake for more detailed documentation

option(DEVEL_DOCS "Enable detailed Doxygen documentation")

file(GLOB libthumbnailer_headers "${PROJECT_SOURCE_DIR}/include/libthumbnailer/*.h")

find_package(Doxygen)
find_program(DOT_EXECUTABLE dot /usr/bin)
if (NOT DOXYGEN_FOUND OR NOT DOT_EXECUTABLE)
    message(WARNING "Cannot generate documentation: doxygen and/or graphviz not found")
else()
    configure_file(${PROJECT_SOURCE_DIR}/doc/libthumbnailer/Doxyfile.in ${PROJECT_BINARY_DIR}/doc/libthumbnailer/Doxyfile @ONLY IMMEDIATE)
    configure_file(${PROJECT_SOURCE_DIR}/doc/libthumbnailer/index.html ${PROJECT_BINARY_DIR}/doc/libthumbnailer/index.html COPYONLY)
    add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/doc/thumbnailer/index.html
                       COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/doc/libthumbnailer/Doxyfile
                       DEPENDS ${PROJECT_BINARY_DIR}/doc/libthumbnailer/Doxyfile
                               ${PROJECT_SOURCE_DIR}/doc/libthumbnailer/tutorial.dox
                               ${libthumbnailer_headers})
    add_custom_target(doc ALL
                       DEPENDS ${PROJECT_BINARY_DIR}/doc/thumbnailer/index.html)
    install(DIRECTORY ${PROJECT_BINARY_DIR}/doc/thumbnailer
            DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/doc)
endif()
