# Relocatable precompiled SDK. No engine source is compiled by consumers.
get_filename_component(_ymgre_sdk "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED YMGRE_INDEX_BITS)
    set(YMGRE_INDEX_BITS 16 CACHE STRING "YMGRE mesh index width")
endif()
if(NOT YMGRE_INDEX_BITS STREQUAL "16" AND NOT YMGRE_INDEX_BITS STREQUAL "32")
    message(FATAL_ERROR "YMGRE_INDEX_BITS must be 16 or 32")
endif()
if(DEFINED YMGRE_CAMERA_COLOR_DEPTH AND NOT YMGRE_CAMERA_COLOR_DEPTH STREQUAL "16")
    message(FATAL_ERROR "This precompiled SDK uses RGB565 (YMGRE_CAMERA_COLOR_DEPTH=16)")
endif()
if(TARGET YMGRE::ymgre)
    get_target_property(_ymgre_bits YMGRE::ymgre YMGRE_SDK_INDEX_BITS)
    if(NOT _ymgre_bits STREQUAL "${YMGRE_INDEX_BITS}")
        message(FATAL_ERROR "Use one YMGRE index width per consumer build directory")
    endif()
else()
add_library(YMGRE::ymgre STATIC IMPORTED)
set_target_properties(YMGRE::ymgre PROPERTIES
    IMPORTED_LOCATION "${_ymgre_sdk}/lib/libymgre_index${YMGRE_INDEX_BITS}.a"
    YMGRE_SDK_INDEX_BITS "${YMGRE_INDEX_BITS}"
    INTERFACE_COMPILE_DEFINITIONS "YMGRE_INDEX_BITS=${YMGRE_INDEX_BITS};YMGRE_CAMERA_COLOR_DEPTH=16"
    INTERFACE_INCLUDE_DIRECTORIES "${_ymgre_sdk}/include/YMGRE/CONFIG;${_ymgre_sdk}/include/YMGRE/CORE;${_ymgre_sdk}/include/YMGRE/OPOBJ;${_ymgre_sdk}/include/YMGRE/IOFILE;${_ymgre_sdk}/include/YMGRE/DEBUG"
    INTERFACE_LINK_LIBRARIES "m")
endif()
# Window integration belongs to the example/application layer.
if(YMGRE_FIND_COMPONENTS)
    set(YMGRE_FOUND FALSE)
    set(YMGRE_NOT_FOUND_MESSAGE "YMGRE exports only the core library. Link an external YMGUI package in your application for windowed demos.")
    return()
endif()
set(YMGRE_FOUND TRUE)
