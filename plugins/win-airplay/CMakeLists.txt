project(win-airplay)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)
include_directories(${FFMPEG_INCLUDE_DIRS})

set(win-airplay_SOURCES
	airplay-server.cpp
	VideoDecoder.cpp
	ipc.c
)
			 
set(win-airplay_HEADERS
	airplay-server.h
	Airplay2Def.h
	common-define.h
	VideoDecoder.h
	ipc.h
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
	${FFMPEG_LIBRARIES})

install_obs_plugin(win-airplay) 