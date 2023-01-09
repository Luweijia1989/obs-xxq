project(rtmp-server)
cmake_minimum_required(VERSION 3.1.3)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "64bit/")
	set(_lib_bit_suffix "x64")
else()
	set(_bit_suffix "32bit/")
	set(_lib_bit_suffix "x86")
endif()

set(CMAKE_CXX_STANDARD 11)

set(MediaKit_Root ${PROJECT_SOURCE_DIR}/src)
set(ToolKit_Root ${PROJECT_SOURCE_DIR}/third-party/ZLToolKit/src)

INCLUDE_DIRECTORIES(${MediaKit_Root})
INCLUDE_DIRECTORIES(${ToolKit_Root})
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/third-party/include)

set(VS_FALGS "/wd4819")

file(GLOB MediaKit_src_list ${MediaKit_Root}/*.cpp ${MediaKit_Root}/*/*.cpp ${MediaKit_Root}/*/*.h ${MediaKit_Root}/*/*.c)
file(GLOB ToolKit_src_list ${ToolKit_Root}/*/*.cpp ${ToolKit_Root}/*/*.h ${ToolKit_Root}/*/*.c)

add_definitions(-DWIN32_LEAN_AND_MEAN -DMP4V2_NO_STDINT_DEFS -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS)

add_executable(rtmp-server ${MediaKit_src_list} ${ToolKit_src_list})

target_link_libraries(rtmp-server
	debug ${PROJECT_SOURCE_DIR}/third-party/lib/${_lib_bit_suffix}/debug/fdk-aac.lib
	optimized ${PROJECT_SOURCE_DIR}/third-party/lib/${_lib_bit_suffix}/relwithdebinfo/fdk-aac.lib
	mirror-ipc)
set_target_properties(rtmp-server PROPERTIES FOLDER "plugins/win-airplay")

install_obs_core(rtmp-server)
