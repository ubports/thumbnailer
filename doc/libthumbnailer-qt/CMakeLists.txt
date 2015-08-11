include(UseDoxygen OPTIONAL)

file(GLOB libthumbnailer_headers "${PROJECT_SOURCE_DIR}/include/libthumbnailer-qt/*.h")

add_doxygen(
    libthumbnailer-qt-doc
    INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/tutorial.dox
        ${libthumbnailer_headers}
    OUTPUT_DIRECTORY
        ${CMAKE_BINARY_DIR}/doc
    STRIP_FROM_PATH
        "${CMAKE_SOURCE_DIR}/src"
    STRIP_FROM_INC_PATH
        "${CMAKE_SOURCE_DIR}/include"
    EXCLUDE_PATTERNS
        */internal/*
    EXCLUDE_SYMBOLS
        *::internal*
        *::Priv
    ALL
)
