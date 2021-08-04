project(win-airplay)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)
include_directories(${FFMPEG_INCLUDE_DIRS}
		${PROJECT_SOURCE_DIR}/portaudio/include
		${PROJECT_SOURCE_DIR}/video-renderer/include)

set(win-airplay_SOURCES
	airplay-server.cpp
	VideoDecoder.cpp
	ipc.c
	d3d11va_decoder.cc
	d3d11va_renderer.cc
	main_window.cc
	Resource.rc
)
			 
set(win-airplay_HEADERS
	airplay-server.h
	Airplay2Def.h
	common-define.h
	VideoDecoder.h
	ipc.h
	d3d11va_decoder.h
	d3d11va_renderer.h
	main_window.h
	av_log.h
	resource.h
)

add_subdirectory(${PROJECT_SOURCE_DIR}/airplay2)
add_subdirectory(${PROJECT_SOURCE_DIR}/iosusbmirror)
add_subdirectory(${PROJECT_SOURCE_DIR}/drivertool)
add_subdirectory(${PROJECT_SOURCE_DIR}/androidusbmirror)

add_library(win-airplay MODULE
	${win-airplay_SOURCES}
	${win-airplay_HEADERS})
target_link_libraries(win-airplay
	Ws2_32
	libobs
	w32-pthreads
	Shlwapi
	D3D11
	debug ${PROJECT_SOURCE_DIR}/portaudio/lib/debug/portaudio_static_x86.lib
	optimized ${PROJECT_SOURCE_DIR}/portaudio/lib/release/portaudio_static_x86.lib
	debug ${PROJECT_SOURCE_DIR}/video-renderer/lib/debug/video-renderer.lib
	optimized ${PROJECT_SOURCE_DIR}/video-renderer/lib/release/video-renderer.lib
	${FFMPEG_LIBRARIES})

install_obs_plugin(win-airplay) 
