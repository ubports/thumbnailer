
set(QML_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/qt5/qml/Ubuntu/Thumbnailer.0.1")

add_library(thumbnailer-qml MODULE
  albumartgenerator.cpp
  artistartgenerator.cpp
  plugin.cpp
  thumbnailerimageresponse.cpp
  thumbnailgenerator.cpp
  ${CMAKE_SOURCE_DIR}/src/ratelimiter.cpp
  ${CMAKE_SOURCE_DIR}/src/settings.cpp
)
include_directories(thumbnailer-qml ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/src)

set_target_properties(thumbnailer-qml PROPERTIES AUTOMOC TRUE NO_SONAME TRUE)
qt5_use_modules(thumbnailer-qml Qml Quick DBus)
#<<<<<<< TREE
#target_link_libraries(thumbnailer-qml Qt5::Qml Qt5::Quick Qt5::DBus ${GIO_DEPS_LDFLAGS})
#if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#    set_source_files_properties(moc_thumbnailerimageresponse.cpp PROPERTIES COMPILE_FLAGS -Wno-old-style-cast)
#    set_source_files_properties(moc_plugin.cpp PROPERTIES COMPILE_FLAGS -Wno-old-style-cast)
#endif()
#=======
target_link_libraries(thumbnailer-qml ${LIBTHUMBNAILER_QT} Qt5::Qml Qt5::Quick Qt5::DBus ${GIO_DEPS_LDFLAGS})
#>>>>>>> MERGE-SOURCE

configure_file(qmldir qmldir)

install(
  TARGETS thumbnailer-qml
  LIBRARY DESTINATION ${QML_PLUGIN_DIR}
)

install(
  FILES qmldir
  DESTINATION ${QML_PLUGIN_DIR}
)
