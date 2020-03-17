if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libdshowcapture/dshowcapture.hpp")
	message(STATUS "libdshowcapture submodule not found!  Please fetch submodules.  win-dshow plugin disabled.")
	return()
endif()

project(win-dshow)

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil swscale)
find_package(Qt5Widgets REQUIRED)
include_directories(${FFMPEG_INCLUDE_DIRS})
include_directories(SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/facesticker/sensetime/include")
include_directories(SYSTEM "${CMAKE_CURRENT_SOURCE_DIR}/facesticker/opengl/include")

set(win-dshow_HEADERS
	encode-dstr.hpp
	ffmpeg-decode.h
	facesticker/st-function.h
	facesticker/st-helper.h
	facesticker/st-thread.h
	facesticker/atomicops.h
	facesticker/readerwriterqueue.h)

set(win-dshow_SOURCES
	win-dshow.cpp
	win-dshow-encoder.cpp
	dshow-plugin.cpp
	ffmpeg-decode.c
	facesticker/st-function.cpp
	facesticker/st-thread.cpp
	facesticker/st-thread-opengl.cpp)

set(libdshowcapture_SOURCES
	libdshowcapture/source/capture-filter.cpp
	libdshowcapture/source/output-filter.cpp
	libdshowcapture/source/dshowcapture.cpp
	libdshowcapture/source/dshowencode.cpp
	libdshowcapture/source/device.cpp
	libdshowcapture/source/encoder.cpp
	libdshowcapture/source/dshow-base.cpp
	libdshowcapture/source/dshow-demux.cpp
	libdshowcapture/source/dshow-enum.cpp
	libdshowcapture/source/dshow-formats.cpp
	libdshowcapture/source/dshow-media-type.cpp
	libdshowcapture/source/dshow-encoded-device.cpp
	libdshowcapture/source/log.cpp)

set(libdshowcapture_HEADERS
	libdshowcapture/dshowcapture.hpp
	libdshowcapture/source/external/IVideoCaptureFilter.h
	libdshowcapture/source/capture-filter.hpp
	libdshowcapture/source/output-filter.hpp
	libdshowcapture/source/device.hpp
	libdshowcapture/source/encoder.hpp
	libdshowcapture/source/dshow-base.hpp
	libdshowcapture/source/dshow-demux.hpp
	libdshowcapture/source/dshow-device-defs.hpp
	libdshowcapture/source/dshow-enum.hpp
	libdshowcapture/source/dshow-formats.hpp
	libdshowcapture/source/dshow-media-type.hpp
	libdshowcapture/source/log.hpp)

add_library(win-dshow MODULE
	${win-dshow_SOURCES}
	${win-dshow_HEADERS}
	${libdshowcapture_SOURCES}
	${libdshowcapture_HEADERS})
target_link_libraries(win-dshow
	Qt5::Widgets
	libobs
	strmiids
	ksuser
	wmcodecdspuuid
	"${CMAKE_CURRENT_SOURCE_DIR}/facesticker/sensetime/libs/st_mobile.lib"
	"${CMAKE_CURRENT_SOURCE_DIR}/facesticker/opengl/libs/gl3w.lib"
	"${CMAKE_CURRENT_SOURCE_DIR}/facesticker/opengl/libs/glfw3.lib"
	${FFMPEG_LIBRARIES})

install_obs_plugin_with_data(win-dshow data)
