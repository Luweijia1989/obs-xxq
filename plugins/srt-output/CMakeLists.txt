project(srt-output)

if(APPLE)
	set(_bit_suffix "")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "64bit/")
	set(_lib_bit_suffix "x64")
else()
	set(_bit_suffix "32bit/")
	set(_lib_bit_suffix "x86")
endif()

set(srt-output_HEADERS
	srt/access_control.h
	srt/logging_api.h
	srt/platform_sys.h
	srt/srt.h
	srt/udt.h
	srt/version.h
	srt/win/syslog_defs.h
	srt/win/unistd.h
	srt-helpers.h
	amf.h
	bytes.h
	flv-mux.h)

set(srt-output_SOURCES
	srt-plugins.c
	srt-output.c
	amf.c
	flv-mux.c)

add_library(srt-output MODULE
	${srt-output_HEADERS}
	${srt-output_SOURCES})
target_link_libraries(srt-output
	libobs
	ws2_32
	w32-pthreads
	crypt32
	debug ${PROJECT_SOURCE_DIR}/srt/lib/${_lib_bit_suffix}/debug/srt_static.lib
	optimized ${PROJECT_SOURCE_DIR}/srt/lib/${_lib_bit_suffix}/release/srt_static.lib
	debug ${PROJECT_SOURCE_DIR}/srt/lib/${_lib_bit_suffix}/debug/libssl.lib
	optimized ${PROJECT_SOURCE_DIR}/srt/lib/${_lib_bit_suffix}/release/libssl.lib
	debug ${PROJECT_SOURCE_DIR}/srt/lib/${_lib_bit_suffix}/debug/libcrypto.lib
	optimized ${PROJECT_SOURCE_DIR}/srt/lib/${_lib_bit_suffix}/release/libcrypto.lib)
set_target_properties(srt-output PROPERTIES FOLDER "plugins")

install_obs_plugin(srt-output data)
