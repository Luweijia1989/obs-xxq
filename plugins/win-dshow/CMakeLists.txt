if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libdshowcapture/dshowcapture.hpp")
	message(STATUS "libdshowcapture submodule not found!  Please fetch submodules.  win-dshow plugin disabled.")
	return()
endif()
  
project(win-dshow)

find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil swscale)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)
include_directories(${FFMPEG_INCLUDE_DIRS})
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/facesticker/bytedance/include")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/facesticker/bytedance/angle_dynamic/inc")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/facesticker/bytedance/angle/include")
include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/facesticker/beautifysource")

add_definitions("-DEGL_EGL_PROTOTYPES=0")
add_definitions("-DGL_GLES_PROTOTYPES=0")
add_definitions("-DUSE_ANGLE")
add_definitions("-DUSE_DYNAMIC_ANGLE")
 
set(CMAKE_AUTOMOC TRUE)  

FILE(GLOB_RECURSE BEAUTIFY_HEADER_FILES ${PROJECT_SOURCE_DIR}/facesticker/beautifysource/*.h)
FILE(GLOB_RECURSE BEAUTIFY_SOURCE_FILES ${PROJECT_SOURCE_DIR}/facesticker/beautifysource/*.cpp)
  
set(BEAUTIFY_SRCS
	${BEAUTIFY_HEADER_FILES}
	${BEAUTIFY_SOURCE_FILES}
	)
   	 
source_group("beautify" FILES ${BEAUTIFY_SRCS})
  
set(win-dshow_HEADERS
	win-dshow.h
	encode-dstr.hpp
	ffmpeg-decode.h
	facesticker/bd-thread.h
	facesticker/atomicops.h
	facesticker/readerwriterqueue.h
	)
  
set(win-dshow_SOURCES
	win-dshow.cpp
	win-dshow-encoder.cpp
	dshow-plugin.cpp
	ffmpeg-decode.c
	facesticker/bd-thread.cpp)

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
	libdshowcapture/source/IVideoCaptureFilter.h
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
	${BEAUTIFY_SRCS}
	${win-dshow_SOURCES}
	${win-dshow_HEADERS}
	${libdshowcapture_SOURCES}
	${libdshowcapture_HEADERS})
target_link_libraries(win-dshow
	Qt5::Widgets
	Qt5::Network
	libobs
	strmiids
	ksuser
	wmcodecdspuuid
	"${PROJECT_SOURCE_DIR}/facesticker/bytedance/libs/effect.lib"
	"${PROJECT_SOURCE_DIR}/facesticker/bytedance/libs/pthreadVC2.lib"
	debug ${PROJECT_SOURCE_DIR}/facesticker/bytedance/angle_dynamic/x86/Debug/dynamic_angle.lib
	optimized ${PROJECT_SOURCE_DIR}/facesticker/bytedance/angle_dynamic/x86/Release/dynamic_angle.lib
	${FFMPEG_LIBRARIES})

install_obs_plugin_with_data(win-dshow data)
