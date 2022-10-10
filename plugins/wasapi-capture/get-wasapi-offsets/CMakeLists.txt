project(get-wasapi-offsets)

set(get-wasapi-offsets_HEADERS
	../wasapi-hook-info.h)

set(get-wasapi-offsets_SOURCES
	get-wasapi-offsets.cpp)

if(MSVC)
	add_compile_options($<IF:$<CONFIG:Debug>,/MTd,/MT>)
endif()

add_executable(get-wasapi-offsets
	${get-wasapi-offsets_SOURCES}
	${get-wasapi-offsets_HEADERS})

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_output_suffix "64")
else()
	set(_output_suffix "32")
endif()

set_target_properties(get-wasapi-offsets
	PROPERTIES
		FOLDER "plugins/wasapi-capture"
		OUTPUT_NAME "get-wasapi-offsets${_output_suffix}")

install_obs_datatarget(get-wasapi-offsets "obs-plugins/wasapi-capture")
