# Relocatable precompiled SDK. No engine source is compiled by consumers.
get_filename_component(_ymgre_sdk "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED YMGRE_INDEX_BITS)
    set(YMGRE_INDEX_BITS 16 CACHE STRING "YMGRE mesh index width")
endif()
if(NOT YMGRE_INDEX_BITS STREQUAL "16" AND NOT YMGRE_INDEX_BITS STREQUAL "32")
    message(FATAL_ERROR "YMGRE_INDEX_BITS must be 16 or 32")
endif()
if(NOT DEFINED YMGRE_CAMERA_COLOR_DEPTH)
    set(YMGRE_CAMERA_COLOR_DEPTH 16 CACHE STRING "Precompiled framebuffer depth")
endif()
if(NOT YMGRE_CAMERA_COLOR_DEPTH STREQUAL "16" AND NOT YMGRE_CAMERA_COLOR_DEPTH STREQUAL "24")
    message(FATAL_ERROR "YMGRE_CAMERA_COLOR_DEPTH must be 16 or 24")
endif()
if(YMGRE_CAMERA_COLOR_DEPTH STREQUAL "16")
    set(_ymgre_format rgb565)
else()
    set(_ymgre_format rgb888)
endif()
set(_ymgre_archive "${_ymgre_sdk}/lib/libymgre_${_ymgre_format}_index${YMGRE_INDEX_BITS}.a")
if(NOT EXISTS "${_ymgre_archive}")
    message(FATAL_ERROR "Missing YMGRE SDK variant: ${_ymgre_archive}")
endif()
if(TARGET YMGRE::ymgre)
    get_target_property(_ymgre_bits YMGRE::ymgre YMGRE_SDK_INDEX_BITS)
    get_target_property(_ymgre_depth YMGRE::ymgre YMGRE_SDK_COLOR_DEPTH)
    get_target_property(_ymgre_location YMGRE::ymgre IMPORTED_LOCATION)
    if(NOT _ymgre_bits STREQUAL "${YMGRE_INDEX_BITS}" OR
       NOT _ymgre_depth STREQUAL "${YMGRE_CAMERA_COLOR_DEPTH}" OR
       NOT _ymgre_location STREQUAL "${_ymgre_archive}")
        message(FATAL_ERROR "Use one YMGRE SDK, index width and color depth per consumer build")
    endif()
else()
    add_library(YMGRE::ymgre STATIC IMPORTED)
    set_target_properties(YMGRE::ymgre PROPERTIES
        IMPORTED_LOCATION "${_ymgre_archive}"
        YMGRE_SDK_INDEX_BITS "${YMGRE_INDEX_BITS}"
        YMGRE_SDK_COLOR_DEPTH "${YMGRE_CAMERA_COLOR_DEPTH}"
        INTERFACE_COMPILE_DEFINITIONS "YMGRE_INDEX_BITS=${YMGRE_INDEX_BITS};YMGRE_CAMERA_COLOR_DEPTH=${YMGRE_CAMERA_COLOR_DEPTH}"
        INTERFACE_COMPILE_FEATURES c_std_99
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
