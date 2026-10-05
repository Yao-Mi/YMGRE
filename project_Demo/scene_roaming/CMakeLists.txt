cmake_minimum_required(VERSION 3.16)
project(scene_roaming C)

include(${CMAKE_CURRENT_LIST_DIR}/../ymgre_app.cmake)

add_executable(scene_roaming scene_roaming.c)
target_link_libraries(scene_roaming PRIVATE ymgre ymgre_demo_host)

ymgre_set_program_output(scene_roaming)
