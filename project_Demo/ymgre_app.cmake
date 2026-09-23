# Shared source-build integration for standalone applications.
# Applications declare their own sources and additional dependencies after this include.
include_guard(GLOBAL)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(YMGRE_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(YMGRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YMGRE_BUILD_YMGUI_HOST ON CACHE BOOL "" FORCE)
get_filename_component(YMGRE_APP_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_subdirectory("${YMGRE_APP_REPO_ROOT}" "${CMAKE_BINARY_DIR}/ymgre")
