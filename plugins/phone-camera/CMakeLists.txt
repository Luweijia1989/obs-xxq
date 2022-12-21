project(phone-camera)

find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil)
include_directories(${FFMPEG_INCLUDE_DIRS})

set(CMAKE_AUTOMOC TRUE)

set(phone-camera_HEADERS
	phone-camera.h
	ffmpeg-decode.h
	common.h
	xxq-tcp.h)

set(phone-camera_SOURCES
	xxq-tcp.cpp
	phone-camera.cpp
	ffmpeg-decode.c
	plugin-main.cpp)

add_library(phone-camera MODULE
	${phone-camera_SOURCES}
	${phone-camera_HEADERS})

target_link_libraries(phone-camera
	${FFMPEG_LIBRARIES}
	libobs
	Qt5::Gui
	Qt5::Widgets
	Qt5::Network
	Ws2_32
	)

install_obs_plugin(phone-camera)

add_subdirectory(driver-installer)
add_subdirectory(mobile-media-backend)
