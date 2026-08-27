cmake_minimum_required(VERSION 3.16)
project(scene_roaming C)

set(YMGRE_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(YMGRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YMGRE_BUILD_YMGUI_HOST ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../.. ${CMAKE_BINARY_DIR}/ymgre)

add_executable(scene_roaming scene_roaming.c
	${CMAKE_CURRENT_LIST_DIR}/../../YMGRE/CORE/YMGRE_SceneManager.c)
target_link_libraries(scene_roaming PRIVATE ymgre ymgre_demo_host)
