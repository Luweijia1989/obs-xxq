project(lyric-hook)

set(lyric-hook_HEADERS
	GlobalCaptureStuff.h
	GraphicsCaptureHook.h)

set(lyric-hook_SOURCES
	GraphicsCaptureHook.cpp
	LyricCapture.cpp)

if(MSVC)
	add_compile_options("$<IF:$<CONFIG:Debug>,/MTd,/MT>")
endif()

add_library(lyric-hook MODULE
	${lyric-hook_SOURCES}
	${lyric-hook_HEADERS})

target_link_libraries(lyric-hook
	psapi)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_output_suffix "64")
else()
	set(_output_suffix "32")
endif()

set_target_properties(lyric-hook
	PROPERTIES
		OUTPUT_NAME "lyric-hook${_output_suffix}")

install_obs_datatarget(lyric-hook "obs-plugins/win-capture")
