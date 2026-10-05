cmake_minimum_required(VERSION 3.16)
project(scene_editor C)

include(${CMAKE_CURRENT_LIST_DIR}/../ymgre_app.cmake)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../extern_lib/mxml ${CMAKE_BINARY_DIR}/mxml)

add_executable(scene_editor
	scene_editor.c
	scene_editor_place.c
	scene_editor_color.c
	scene_editor_font.c
	scene_editor_inspector.c
	scene_editor_import.c
	scene_editor_file_dialog.c
	scene_editor_io.c
	scene_editor_model.c
	scene_editor_hierarchy.c
)
target_compile_definitions(scene_editor PRIVATE
	GB2312_BIN_PATH="${CMAKE_CURRENT_LIST_DIR}/../../extern_lib/YMGUI/Demo/gb2312_glyphs.bin"
)
target_link_libraries(scene_editor PRIVATE ymgre ymgre_demo_host MiniXML::MiniXML)

ymgre_set_program_output(scene_editor)
