cmake_minimum_required(VERSION 3.4.1)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "x64")
else()
	set(_bit_suffix "x86")
endif()

find_package(Qt5Core REQUIRED)
set(CMAKE_AUTOMOC TRUE)

set(mirror-ipc_SOURCES
	comm-ipc.cpp
	ipc-private.cpp
	ipc.c)

set(mirror-ipc_HEADERS
	common-define.h
	comm-ipc.h
	ipc-private.h
	ipc.h)

include_directories(.)
include_directories(./include)
add_library(mirror-ipc
        STATIC
        ${mirror-ipc_SOURCES}
	${mirror-ipc_HEADERS})

target_link_libraries(mirror-ipc
	Qt5::Core
	libobs
	debug ${PROJECT_SOURCE_DIR}/mirror-ipc/lib/${_bit_suffix}/debug/RPC.lib
	optimized ${PROJECT_SOURCE_DIR}/mirror-ipc/lib/${_bit_suffix}/release/RPC.lib
)

target_include_directories(mirror-ipc
	PUBLIC .)
