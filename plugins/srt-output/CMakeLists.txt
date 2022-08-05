project(srt-output)

find_package(Libsrt REQUIRED)

if(APPLE)
	set(_bit_suffix "")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "64bit/")
	set(_lib_bit_suffix "x64")
else()
	set(_bit_suffix "32bit/")
	set(_lib_bit_suffix "x86")
endif()

set(srt-output_SOURCES
	srt-plugins.cpp
	srt-output.cpp)

add_library(srt-output MODULE
	${srt-output_SOURCES})
target_link_libraries(srt-output
	libobs
	ws2_32
	w32-pthreads
	crypt32
	debug ${PROJECT_SOURCE_DIR}/libmpeg/lib/${_lib_bit_suffix}/debug/libmpeg.lib
	optimized ${PROJECT_SOURCE_DIR}/libmpeg/lib/${_lib_bit_suffix}/release/libmpeg.lib
	Libsrt::Libsrt)
set_target_properties(srt-output PROPERTIES FOLDER "plugins")

install_obs_plugin(srt-output data)
