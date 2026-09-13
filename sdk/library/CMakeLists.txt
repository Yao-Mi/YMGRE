cmake_minimum_required(VERSION 3.16)
project(YMGRE_SDK_Libraries C)
set(YMGRE_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(YMGRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YMGRE_BUILD_YMGUI_HOST OFF CACHE BOOL "" FORCE)
set(YMGRE_CAMERA_COLOR_DEPTH 16 CACHE STRING "" FORCE)
add_subdirectory(../.. engine)
# Ship the full public scene API in the archive, not as application source.
target_sources(ymgre PRIVATE ../../YMGRE/CORE/YMGRE_SceneManager.c)
set_target_properties(ymgre PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
