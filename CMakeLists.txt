cmake_minimum_required(VERSION 3.16)
project(YMGRE_YMGUI_Demo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(YMGRE_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(YMGUI_ROOT ${YMGRE_REPO_ROOT}/extern_lib/YMGUI)

option(YMGRE_BUILD_DEMOS "Build YMGRE basic demos" ON)
option(YMGRE_BUILD_TESTS "Build YMGRE module tests" ON)
option(YMGRE_BUILD_YMGUI_HOST "Build the YMGUI/SDL demo host" ${YMGRE_BUILD_DEMOS})
set(YMGRE_CAMERA_COLOR_DEPTH 24 CACHE STRING "Camera framebuffer color depth (24 RGB888 for PC, 16 RGB565 for MCU)")
set_property(CACHE YMGRE_CAMERA_COLOR_DEPTH PROPERTY STRINGS 16 24)
if(NOT YMGRE_CAMERA_COLOR_DEPTH STREQUAL "16" AND NOT YMGRE_CAMERA_COLOR_DEPTH STREQUAL "24")
	message(FATAL_ERROR "YMGRE_CAMERA_COLOR_DEPTH must be 16 or 24")
endif()

if(YMGRE_BUILD_DEMOS)
	set(YMGRE_BUILD_YMGUI_HOST ON)
endif()

if(YMGRE_BUILD_YMGUI_HOST)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SDL2 REQUIRED sdl2)

	file(GLOB YMGUI_SRC CONFIGURE_DEPENDS
		${YMGUI_ROOT}/YMGUI/CONFIG/*.c
		${YMGUI_ROOT}/YMGUI/DEBUG/*.c
		${YMGUI_ROOT}/YMGUI/COMMON/GEOM/*.c
		${YMGUI_ROOT}/YMGUI/COMMON/MATH/*.c
		${YMGUI_ROOT}/YMGUI/OPOBJ/*.c
		${YMGUI_ROOT}/YMGUI/CORE/*.c
		${YMGUI_ROOT}/YMGUI/GUI/*.c
		${YMGUI_ROOT}/YMGUI/WIDGET/*.c
		${YMGUI_ROOT}/YMGUI/STATE/*.c
		${YMGUI_ROOT}/YMGUI/HAL/*.c
	)
	add_library(ymgui STATIC ${YMGUI_SRC})
	target_compile_definitions(ymgui PUBLIC
		YMGUI_COLOR_DEPTH=${YMGRE_CAMERA_COLOR_DEPTH}
	)
	target_include_directories(ymgui PUBLIC
		${YMGUI_ROOT}/YMGUI/CONFIG
		${YMGUI_ROOT}/YMGUI/DEBUG
		${YMGUI_ROOT}/YMGUI/COMMON/GEOM
		${YMGUI_ROOT}/YMGUI/COMMON/MATH
		${YMGUI_ROOT}/YMGUI/OPOBJ
		${YMGUI_ROOT}/YMGUI/CORE
		${YMGUI_ROOT}/YMGUI/GUI
		${YMGUI_ROOT}/YMGUI/WIDGET
		${YMGUI_ROOT}/YMGUI/STATE
		${YMGUI_ROOT}/YMGUI/HAL
	)

	add_library(ymgui_sdl STATIC ${YMGUI_ROOT}/SDL_LCD/SDL_LCD.c)
	target_include_directories(ymgui_sdl PRIVATE ${SDL2_INCLUDE_DIRS} ${YMGUI_ROOT}/SDL_LCD)
	target_link_libraries(ymgui_sdl PUBLIC ymgui ${SDL2_LIBRARIES})
endif()

file(GLOB YMGRE_SRC CONFIGURE_DEPENDS
	YMGRE/CONFIG/*.c
	YMGRE/CORE/*.c
	YMGRE/DEBUG/*.c
	YMGRE/IOFILE/*.c
	YMGRE/OPOBJ/*.c
)
list(REMOVE_ITEM YMGRE_SRC
	${YMGRE_REPO_ROOT}/YMGRE/CORE/YMGRE_SceneManager.c
)
add_library(ymgre STATIC ${YMGRE_SRC})
target_include_directories(ymgre PUBLIC
	YMGRE/CONFIG YMGRE/CORE YMGRE/DEBUG YMGRE/IOFILE YMGRE/OPOBJ
)
target_compile_definitions(ymgre PUBLIC
	YMGRE_CAMERA_COLOR_DEPTH=${YMGRE_CAMERA_COLOR_DEPTH}
)
target_link_libraries(ymgre PUBLIC m)

if(YMGRE_BUILD_YMGUI_HOST)
	add_library(ymgre_demo_host STATIC Demo/demo_host.c)
	target_include_directories(ymgre_demo_host PUBLIC
		Demo ${YMGUI_ROOT}/SDL_LCD ${SDL2_INCLUDE_DIRS}
	)
	target_link_libraries(ymgre_demo_host PUBLIC ymgre ymgui_sdl)
endif()

if(YMGRE_BUILD_DEMOS)
	add_executable(demo_basic_shapes Demo/demo_basic_shapes.c)
	target_link_libraries(demo_basic_shapes PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_extended_shapes Demo/demo_extended_shapes.c)
	target_link_libraries(demo_extended_shapes PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_stereo_view Demo/demo_stereo_view.c)
	target_link_libraries(demo_stereo_view PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_polygon_fill Demo/demo_polygon_fill.c)
	target_link_libraries(demo_polygon_fill PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_fragment_stitching Demo/demo_fragment_stitching.c)
	target_link_libraries(demo_fragment_stitching PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_depth_overlap Demo/demo_depth_overlap.c)
	target_link_libraries(demo_depth_overlap PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_frustum_clipping Demo/demo_frustum_clipping.c)
	target_link_libraries(demo_frustum_clipping PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_shared_edge_stress Demo/demo_shared_edge_stress.c)
	target_link_libraries(demo_shared_edge_stress PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_backface_culling Demo/demo_backface_culling.c)
	target_link_libraries(demo_backface_culling PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_camera_viewport Demo/demo_camera_viewport.c)
	target_link_libraries(demo_camera_viewport PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_window_clipping Demo/demo_window_clipping.c)
	target_link_libraries(demo_window_clipping PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_degenerate_geometry Demo/demo_degenerate_geometry.c)
	target_link_libraries(demo_degenerate_geometry PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_lighting Demo/demo_lighting.c)
	target_link_libraries(demo_lighting PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_material_texture Demo/demo_material_texture.c)
	target_link_libraries(demo_material_texture PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_vertex Demo/demo_advanced_vertex.c)
	target_link_libraries(demo_advanced_vertex PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_texture Demo/demo_advanced_texture.c)
	target_link_libraries(demo_advanced_texture PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_stages Demo/demo_advanced_stages.c)
	target_link_libraries(demo_advanced_stages PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_point_light Demo/demo_advanced_point_light.c)
	target_link_libraries(demo_advanced_point_light PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_perspective Demo/demo_advanced_perspective.c)
	target_link_libraries(demo_advanced_perspective PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_perspective_verify Demo/demo_advanced_perspective_verify.c)
	target_link_libraries(demo_advanced_perspective_verify PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_clipping Demo/demo_advanced_clipping.c)
	target_link_libraries(demo_advanced_clipping PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_clipping_planes Demo/demo_advanced_clipping_planes.c)
	target_link_libraries(demo_advanced_clipping_planes PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_depth Demo/demo_advanced_depth.c)
	target_link_libraries(demo_advanced_depth PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_cube Demo/demo_advanced_cube.c)
	target_link_libraries(demo_advanced_cube PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_lighting Demo/demo_advanced_lighting.c)
	target_link_libraries(demo_advanced_lighting PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_specular Demo/demo_advanced_specular.c)
	target_link_libraries(demo_advanced_specular PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_spot_light Demo/demo_advanced_spot_light.c)
	target_link_libraries(demo_advanced_spot_light PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_material_channels Demo/demo_advanced_material_channels.c)
	target_link_libraries(demo_advanced_material_channels PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_specular_texture Demo/demo_advanced_specular_texture.c)
	target_link_libraries(demo_advanced_specular_texture PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_gloss Demo/demo_advanced_gloss.c)
	target_link_libraries(demo_advanced_gloss PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_spot_specular Demo/demo_advanced_spot_specular.c)
	target_link_libraries(demo_advanced_spot_specular PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_light_accumulation Demo/demo_advanced_light_accumulation.c)
	target_link_libraries(demo_advanced_light_accumulation PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_light_order Demo/demo_advanced_light_order.c)
	target_link_libraries(demo_advanced_light_order PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_backlight Demo/demo_advanced_backlight.c)
	target_link_libraries(demo_advanced_backlight PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_normal_specular Demo/demo_advanced_normal_specular.c)
	target_link_libraries(demo_advanced_normal_specular PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_specular_sampling Demo/demo_advanced_specular_sampling.c)
	target_link_libraries(demo_advanced_specular_sampling PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_render_modes Demo/demo_advanced_render_modes.c)
	target_link_libraries(demo_advanced_render_modes PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_normals Demo/demo_advanced_normals.c)
	target_link_libraries(demo_advanced_normals PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_normal_map Demo/demo_advanced_normal_map.c)
	target_link_libraries(demo_advanced_normal_map PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_normal_map_detail Demo/demo_advanced_normal_map_detail.c)
	target_link_libraries(demo_advanced_normal_map_detail PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_advanced_mirror_uv Demo/demo_advanced_mirror_uv.c)
	target_link_libraries(demo_advanced_mirror_uv PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_render_target Demo/demo_render_target.c)
	target_link_libraries(demo_render_target PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_polygon_pipeline Demo/demo_polygon_pipeline.c)
	target_link_libraries(demo_polygon_pipeline PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_raster_primitives Demo/demo_raster_primitives.c)
	target_link_libraries(demo_raster_primitives PRIVATE ymgre ymgre_demo_host)
	add_executable(demo_polygon_triangulation Demo/demo_polygon_triangulation.c)
	target_link_libraries(demo_polygon_triangulation PRIVATE ymgre ymgre_demo_host)
endif()

if(YMGRE_BUILD_TESTS)
	enable_testing()
	add_executable(test_render_context tests/test_render_context.c)
	target_link_libraries(test_render_context PRIVATE ymgre)
	add_test(NAME render_context COMMAND test_render_context)
	add_executable(test_basic_mesh tests/test_basic_mesh.c)
	target_link_libraries(test_basic_mesh PRIVATE ymgre)
	add_test(NAME basic_mesh COMMAND test_basic_mesh)
	add_executable(test_triangle_rasterization tests/test_triangle_rasterization.c)
	target_link_libraries(test_triangle_rasterization PRIVATE ymgre)
	add_test(NAME triangle_rasterization COMMAND test_triangle_rasterization)
	add_executable(test_polygon_rasterization tests/test_polygon_rasterization.c)
	target_link_libraries(test_polygon_rasterization PRIVATE ymgre)
	add_test(NAME polygon_rasterization COMMAND test_polygon_rasterization)
	add_executable(test_depth_buffer tests/test_depth_buffer.c)
	target_link_libraries(test_depth_buffer PRIVATE ymgre)
	add_test(NAME depth_buffer COMMAND test_depth_buffer)
	add_executable(test_frustum_clipping tests/test_frustum_clipping.c)
	target_link_libraries(test_frustum_clipping PRIVATE ymgre)
	add_test(NAME frustum_clipping COMMAND test_frustum_clipping)
	add_executable(test_shared_edge_stress tests/test_shared_edge_stress.c)
	target_link_libraries(test_shared_edge_stress PRIVATE ymgre)
	add_test(NAME shared_edge_stress COMMAND test_shared_edge_stress)
	add_executable(test_backface_culling tests/test_backface_culling.c)
	target_link_libraries(test_backface_culling PRIVATE ymgre)
	add_test(NAME backface_culling COMMAND test_backface_culling)
	add_executable(test_camera_viewport tests/test_camera_viewport.c)
	target_link_libraries(test_camera_viewport PRIVATE ymgre)
	add_test(NAME camera_viewport COMMAND test_camera_viewport)
	add_executable(test_polygon_clipping tests/test_polygon_clipping.c)
	target_link_libraries(test_polygon_clipping PRIVATE ymgre)
	add_test(NAME polygon_clipping COMMAND test_polygon_clipping)
	add_executable(test_degenerate_geometry tests/test_degenerate_geometry.c)
	target_link_libraries(test_degenerate_geometry PRIVATE ymgre)
	add_test(NAME degenerate_geometry COMMAND test_degenerate_geometry)
	add_executable(test_resource_material tests/test_resource_material.c
		YMGRE/CORE/YMGRE_SceneManager.c)
	target_link_libraries(test_resource_material PRIVATE ymgre)
	add_test(NAME resource_material COMMAND test_resource_material
		${YMGRE_REPO_ROOT}/Resource
		${CMAKE_CURRENT_BINARY_DIR}/test_resource_material_roundtrip.bmp)
	add_executable(test_advanced_rasterization tests/test_advanced_rasterization.c)
	target_link_libraries(test_advanced_rasterization PRIVATE ymgre)
	add_test(NAME advanced_rasterization COMMAND test_advanced_rasterization)
	add_executable(test_advanced_clipping tests/test_advanced_clipping.c)
	target_link_libraries(test_advanced_clipping PRIVATE ymgre)
	add_test(NAME advanced_clipping COMMAND test_advanced_clipping)
endif()
