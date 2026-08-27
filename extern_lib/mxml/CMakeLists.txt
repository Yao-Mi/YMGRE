cmake_minimum_required(VERSION 3.16)

add_library(mxml STATIC
	mxml-attr.c
	mxml-file.c
	mxml-get.c
	mxml-index.c
	mxml-node.c
	mxml-options.c
	mxml-private.c
	mxml-search.c
	mxml-set.c
)
add_library(MiniXML::MiniXML ALIAS mxml)

target_include_directories(mxml PUBLIC ${CMAKE_CURRENT_LIST_DIR})
set_target_properties(mxml PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)

find_package(Threads QUIET)
if(Threads_FOUND)
	target_link_libraries(mxml PRIVATE Threads::Threads)
endif()
