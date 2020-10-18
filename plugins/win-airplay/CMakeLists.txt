project(win-airplay)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)
include_directories(${FFMPEG_INCLUDE_DIRS}
		${PROJECT_SOURCE_DIR}/portaudio/include)

set(win-airplay_SOURCES
	airplay-server.cpp
	VideoDecoder.cpp
	ipc.c
	Resource.rc
)
			 
set(win-airplay_HEADERS
	airplay-server.h
	Airplay2Def.h
	common-define.h
	VideoDecoder.h
	ipc.h
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
	debug ${PROJECT_SOURCE_DIR}/portaudio/lib/debug/portaudio_static_x86.lib
	optimized ${PROJECT_SOURCE_DIR}/portaudio/lib/release/portaudio_static_x86.lib
	${FFMPEG_LIBRARIES})

install_obs_plugin(win-airplay) 
