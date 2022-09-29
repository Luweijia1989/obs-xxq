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
	mirror_server.cpp
	dxva2_decoder.cc
	d3d9_renderer.cc
	qrcode/qr_code.cpp
	qrcode/qr_to_png.cpp
	qrcode/tiny_png_out.cpp
	tool.cpp
)
			 
set(win-airplay_HEADERS
	mirror_server.h
	dxva2_decoder.h
	d3d9_renderer.h
	qrcode/qr_code.hpp
	qrcode/qr_to_png.h
	qrcode/tiny_png_out.hpp
	tool.h
)

set(airplay_QRC
	img/resource.qrc)

qt5_add_resources(airplay_QRC_SOURCES ${airplay_QRC})

add_subdirectory(${PROJECT_SOURCE_DIR}/airplay)
add_subdirectory(${PROJECT_SOURCE_DIR}/iosqtvusbmirror)
add_subdirectory(${PROJECT_SOURCE_DIR}/androidusbmirror)
add_subdirectory(${PROJECT_SOURCE_DIR}/androidaoa)
add_subdirectory(${PROJECT_SOURCE_DIR}/rtmp-server)
add_subdirectory(${PROJECT_SOURCE_DIR}/mirror-ipc)

add_library(win-airplay MODULE
	${win-airplay_SOURCES}
	${win-airplay_HEADERS}
	${airplay_QRC_SOURCES})
target_link_libraries(win-airplay
	mirror-ipc
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
set_target_properties(win-airplay PROPERTIES FOLDER "plugins/win-airplay")

install_obs_plugin(win-airplay) 
