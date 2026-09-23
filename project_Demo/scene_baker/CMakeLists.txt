cmake_minimum_required(VERSION 3.16)
project(scene_baker C CXX)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
add_library(scene_uv_atlas STATIC scene_uv_atlas.cpp ../../extern_lib/xatlas/xatlas.cpp)
target_include_directories(scene_uv_atlas PRIVATE ../../extern_lib/xatlas)
target_compile_features(scene_uv_atlas PRIVATE cxx_std_11)
target_compile_definitions(scene_uv_atlas PRIVATE XA_MULTITHREADED=0)
set(YMGRE_CAMERA_COLOR_DEPTH 24 CACHE STRING "Framebuffer color depth")
if(NOT YMGRE_CAMERA_COLOR_DEPTH STREQUAL "24")
  message(FATAL_ERROR "scene_baker currently requires RGB888 (YMGRE_CAMERA_COLOR_DEPTH=24)")
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../ymgre_app.cmake)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../extern_lib/mxml ${CMAKE_BINARY_DIR}/mxml)
add_executable(scene_baker
  scene_bake.c scene_model_export.c scene_ray_renderer.c scene_uv.c scene_uv_data.c scene_uv_edit.c scene_uv_editor.c scene_uv_preview.c scene_editor_texture.c scene_texture_image.c scene_texture_paint.c scene_editor.c scene_editor_place.c scene_editor_color.c scene_editor_font.c
  scene_editor_inspector.c scene_editor_import.c scene_editor_file_dialog.c
  scene_editor_io.c scene_editor_model.c scene_editor_hierarchy.c)
target_compile_definitions(scene_baker PRIVATE
  YMGRE_EARTH_FIXTURE="${CMAKE_CURRENT_LIST_DIR}/../../Resource/地球贴图.bmp"
  YMGRE_TANK_FIXTURE="${CMAKE_CURRENT_LIST_DIR}/../../Resource/obj/Tank1_Head.mesh"
  GB2312_BIN_PATH="${CMAKE_CURRENT_LIST_DIR}/../../extern_lib/YMGUI/Demo/gb2312_glyphs.bin")
target_include_directories(scene_baker PRIVATE .)
target_link_libraries(scene_baker PRIVATE scene_uv_atlas ymgre ymgre_demo_host MiniXML::MiniXML PNG::PNG JPEG::JPEG)

add_executable(test_scene_bake test_scene_bake.c scene_bake.c)
target_link_libraries(test_scene_bake PRIVATE ymgre)
enable_testing()
add_test(NAME scene_bake COMMAND test_scene_bake)
add_test(NAME scene_baker_editor COMMAND scene_baker)
set_tests_properties(scene_baker_editor PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_SCENE_EDITOR_SELFTEST=1;YMGRE_SCENE_BAKER_SELFTEST=1")

add_executable(scene_bake_cli scene_baker.c scene_bake.c)
target_link_libraries(scene_bake_cli PRIVATE ymgre)

add_test(NAME tank_model_roundtrip COMMAND scene_baker)
set_tests_properties(tank_model_roundtrip PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_TANK_ROUNDTRIP=1")

add_test(NAME color_bake_roundtrip COMMAND scene_baker)
set_tests_properties(color_bake_roundtrip PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_COLOR_BAKE_SELFTEST=1")

add_test(NAME specular_bake_roundtrip COMMAND scene_baker)
set_tests_properties(specular_bake_roundtrip PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_SPECULAR_BAKE_SELFTEST=1")

add_test(NAME bake_brightness_preserved COMMAND scene_baker)
set_tests_properties(bake_brightness_preserved PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_SPECULAR_BAKE_SELFTEST=1;YMGRE_BAKE_COMPARE_STRESS=1")

add_executable(test_scene_ray_renderer test_scene_ray_renderer.c scene_ray_renderer.c scene_bake.c)
target_link_libraries(test_scene_ray_renderer PRIVATE ymgre)
add_test(NAME scene_ray_renderer COMMAND test_scene_ray_renderer)
add_test(NAME scene_renderer_switch COMMAND scene_baker)
set_tests_properties(scene_renderer_switch PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_RENDERER_SELFTEST=1")

add_test(NAME scene_optics COMMAND scene_baker)
set_tests_properties(scene_optics PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_OPTICS_SELFTEST=1")

add_test(NAME scene_smooth_sphere COMMAND scene_baker)
set_tests_properties(scene_smooth_sphere PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_SMOOTH_SELFTEST=1")

add_executable(test_scene_uv test_scene_uv.c scene_uv.c scene_uv_data.c scene_uv_edit.c)
target_link_libraries(test_scene_uv PRIVATE scene_uv_atlas ymgre)
add_test(NAME scene_uv COMMAND test_scene_uv)
add_test(NAME scene_uv_editor COMMAND scene_baker)
set_tests_properties(scene_uv_editor PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_UV_SELFTEST=1")

add_test(NAME scene_uv_cylinder COMMAND scene_baker)
set_tests_properties(scene_uv_cylinder PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_UV_SELFTEST=1;YMGRE_UV_CYLINDER_DEMO=1")

add_test(NAME scene_uv_preview COMMAND scene_baker)
set_tests_properties(scene_uv_preview PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_UV_SELFTEST=1;YMGRE_UV_PREVIEW_DEMO=1")

add_test(NAME scene_uv_apply COMMAND scene_baker)
set_tests_properties(scene_uv_apply PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_UV_SELFTEST=1;YMGRE_UV_APPLY_DEMO=1")

add_test(NAME scene_uv_primitives COMMAND scene_baker)
set_tests_properties(scene_uv_primitives PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_UV_SELFTEST=1;YMGRE_UV_PRIMITIVES=1")

add_test(NAME scene_texture_tools COMMAND scene_baker)
set_tests_properties(scene_texture_tools PROPERTIES ENVIRONMENT
  "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;YMGRE_TEXTURE_TOOLS=1")
