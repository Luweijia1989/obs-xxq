project(win-airplay)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample)
include_directories(${FFMPEG_INCLUDE_DIRS}
		${PROJECT_SOURCE_DIR}/video-renderer)

set(CMAKE_CXX_STANDARD 17)

set(win-airplay_SOURCES
	airplay-server.cpp
	ipc.c
	dxva2_decoder.cc
	dxva2_renderer.cc
	video-renderer/d3d9_render_texture.cc
	video-renderer/d3d9_renderer.cc
	video-renderer/d3d11_render_texture.cc
	video-renderer/d3d11_renderer.cc
	qrcode/QrCode.cpp
	qrcode/QrToPng.cpp
	qrcode/TinyPngOut.cpp
	tool.cpp
)
			 
set(win-airplay_HEADERS
	airplay-server.h
	Airplay2Def.h
	common-define.h
	ipc.h
	dxva2_decoder.h
	dxva2_renderer.h
	av_log.h
	video-renderer/d3d9_render_texture.h
	video-renderer/d3d9_renderer.h
	video-renderer/d3d11_render_texture.h
	video-renderer/d3d11_renderer.h
	video-renderer/log.h
	video-renderer/renderer.h
	video-renderer/shader_yuv_bt601.h
	video-renderer/shader_yuv_bt709.h
	qrcode/QrCode.hpp
	qrcode/QrToPng.h
	qrcode/TinyPngOut.hpp
	resource.h
	tool.h
)

set(airplay_QRC
	img/resource.qrc)

qt5_add_resources(airplay_QRC_SOURCES ${airplay_QRC})

add_subdirectory(${PROJECT_SOURCE_DIR}/airplay)
add_subdirectory(${PROJECT_SOURCE_DIR}/iosusbmirror)
add_subdirectory(${PROJECT_SOURCE_DIR}/iosqtvusbmirror)
add_subdirectory(${PROJECT_SOURCE_DIR}/androidusbmirror)
add_subdirectory(${PROJECT_SOURCE_DIR}/androidaoa)
add_subdirectory(${PROJECT_SOURCE_DIR}/rtmp-server)

add_library(win-airplay MODULE
	${win-airplay_SOURCES}
	${win-airplay_HEADERS}
	${airplay_QRC_SOURCES})
target_link_libraries(win-airplay
	Ws2_32
	libobs
	w32-pthreads
	Shlwapi
	D3D11
	D3DCompiler
	D3d9
	Qt5::Gui
	Qt5::Network
	Qt5::Widgets
	${FFMPEG_LIBRARIES})

install_obs_plugin(win-airplay) 
