project(obs-ffmpeg)

if(MSVC)
	set(obs-ffmpeg_PLATFORM_DEPS
		w32-pthreads)
endif()

option(ENABLE_FFMPEG_LOGGING "Enables obs-ffmpeg logging" OFF)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)
include_directories(${FFMPEG_INCLUDE_DIRS})

find_package(Librist REQUIRED)
find_package(Libsrt REQUIRED)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/obs-ffmpeg-config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/obs-ffmpeg-config.h")
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(obs-ffmpeg_config_HEADERS
	"${CMAKE_CURRENT_BINARY_DIR}/obs-ffmpeg-config.h")

set(obs-ffmpeg_HEADERS
	obs-ffmpeg-formats.h
	obs-ffmpeg-compat.h
	obs-ffmpeg-output.h
	obs-ffmpeg-srt.h
	obs-ffmpeg-rist.h
	obs-ffmpeg-url.h)

set(obs-ffmpeg_SOURCES
	obs-ffmpeg.c
	obs-ffmpeg-audio-encoders.c
	obs-ffmpeg-nvenc.c
	obs-ffmpeg-output.c
	obs-ffmpeg-mux.c
	obs-ffmpeg-mpegts.c
	obs-ffmpeg-source.c)

if(UNIX AND NOT APPLE)
	list(APPEND obs-ffmpeg_SOURCES
		obs-ffmpeg-vaapi.c)
	LIST(APPEND obs-ffmpeg_PLATFORM_DEPS
		${LIBVA_LBRARIES})
endif()

if(ENABLE_FFMPEG_LOGGING)
	list(APPEND obs-ffmpeg_SOURCES
		obs-ffmpeg-logging.c)
endif()

if(WIN32)
	add_subdirectory(obs-amf-test)

	list(APPEND obs-ffmpeg_SOURCES
		jim-nvenc.c
		jim-nvenc-helpers.c
		texture-amf.cpp
		texture-amf-opts.hpp)
	list(APPEND obs-ffmpeg_HEADERS
		jim-nvenc.h)
endif()

add_library(obs-ffmpeg MODULE
	${obs-ffmpeg_config_HEADERS}
	${obs-ffmpeg_HEADERS}
	${obs-ffmpeg_SOURCES})
target_link_libraries(obs-ffmpeg
	libobs
	media-playback
	${obs-ffmpeg_PLATFORM_DEPS}
	${FFMPEG_LIBRARIES})
target_link_libraries(obs-ffmpeg Librist::Librist Libsrt::Libsrt)
target_link_libraries(obs-ffmpeg ws2_32.lib)
set_target_properties(obs-ffmpeg PROPERTIES FOLDER "plugins/obs-ffmpeg")

install_obs_plugin_with_data(obs-ffmpeg data)

add_subdirectory(ffmpeg-mux)
