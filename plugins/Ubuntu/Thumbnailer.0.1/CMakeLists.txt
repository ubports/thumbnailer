set(QML_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/qt5/qml/Ubuntu/Thumbnailer.0.1")

include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/src)

add_library(thumbnailer-qml-static STATIC
  albumartgenerator.cpp
  artistartgenerator.cpp
  thumbnailerimageresponse.cpp
  thumbnailgenerator.cpp
  )
set_target_properties(thumbnailer-qml-static PROPERTIES AUTOMOC TRUE)
qt5_use_modules(thumbnailer-qml-static Qml Quick)
target_link_libraries(thumbnailer-qml-static ${LIBTHUMBNAILER_QT} Qt5::Qml Qt5::Quick)

add_library(thumbnailer-qml MODULE
  plugin.cpp
)

set_target_properties(thumbnailer-qml PROPERTIES AUTOMOC TRUE NO_SONAME TRUE)
target_link_libraries(thumbnailer-qml thumbnailer-qml-static)

configure_file(qmldir qmldir)

install(
  TARGETS thumbnailer-qml
  LIBRARY DESTINATION ${QML_PLUGIN_DIR}
)

install(
  FILES qmldir
  DESTINATION ${QML_PLUGIN_DIR}
)
