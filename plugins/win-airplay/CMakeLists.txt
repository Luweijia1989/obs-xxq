project(win-airplay)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)
include_directories(${FFMPEG_INCLUDE_DIRS})

include_directories(
		${PROJECT_SOURCE_DIR}/airplay2/include
		${PROJECT_SOURCE_DIR}/airplay2/lib
        )

set(win-airplay_SOURCES
	airplay-server.cpp
	Airplay2Export.cpp
	CAutoLock.cpp
	FgAirplayServer.cpp
	FgAirplayChannel.cpp
	CAirServer.cpp
	CAirServerCallback.cpp
	FgUtf8Utils.cpp
)
			
set(win-airplay_HEADERS
	airplay-server.h
	Airplay2Def.h
	Airplay2Head.h
	CAutoLock.h
	FgAirplayChannel.h
	FgAirplayServer.h
	airplay-server.h
	CAirServer.h
	CAirServerCallback.h
	FgUtf8Utils.h
)

add_subdirectory(${PROJECT_SOURCE_DIR}/airplay2/lib)

add_library(win-airplay MODULE
	${win-airplay_SOURCES}
	${win-airplay_HEADERS})
target_link_libraries(win-airplay
	Ws2_32
	Winmm
	airplay2
	libobs
	${FFMPEG_LIBRARIES})

install_obs_plugin(win-airplay)