cmake_minimum_required(VERSION 3.16)
project(image_decode C)
option(IMAGE_DECODE_ENABLE_PNG "Decode PNG using libpng" OFF)
option(IMAGE_DECODE_ENABLE_JPEG "Decode JPEG using libjpeg" OFF)
set(IMAGE_DECODE_MAX_BYTES "536870912" CACHE STRING "Maximum decoded buffer size in bytes")

add_library(image_decode_config INTERFACE)
target_include_directories(image_decode_config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(image_decode_config INTERFACE
 IMAGE_DECODE_ENABLE_PNG=$<BOOL:${IMAGE_DECODE_ENABLE_PNG}>
 IMAGE_DECODE_ENABLE_JPEG=$<BOOL:${IMAGE_DECODE_ENABLE_JPEG}>
 IMAGE_DECODE_MAX_BYTES=${IMAGE_DECODE_MAX_BYTES})
if(IMAGE_DECODE_ENABLE_PNG)
 find_package(PNG REQUIRED)
 target_link_libraries(image_decode_config INTERFACE PNG::PNG)
endif()
if(IMAGE_DECODE_ENABLE_JPEG)
 find_package(JPEG REQUIRED)
 target_link_libraries(image_decode_config INTERFACE JPEG::JPEG)
endif()
add_library(image_decode_objects OBJECT image_decode.c)
target_compile_features(image_decode_objects PRIVATE c_std_99)
target_link_libraries(image_decode_objects PUBLIC image_decode_config)
set_target_properties(image_decode_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Standalone consumers link this target; YMGRE embeds the same objects in its SDK.
add_library(image_decode STATIC $<TARGET_OBJECTS:image_decode_objects>)
target_link_libraries(image_decode PUBLIC image_decode_config)
add_library(ImageDecode::image_decode ALIAS image_decode)
