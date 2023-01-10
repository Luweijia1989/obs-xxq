project(phone-camera)

find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(LibEVENT REQUIRED)
find_package(FFmpeg REQUIRED COMPONENTS avcodec avutil)
include_directories(${FFMPEG_INCLUDE_DIRS})

set(CMAKE_AUTOMOC TRUE)

set(phone-camera_HEADERS
	ipc.h
	phone-camera.h
	ffmpeg-decode.h
	common.h
	xxq-tcp.h)

set(phone-camera_SOURCES
	ipc.c
	xxq-tcp.cpp
	phone-camera.cpp
	ffmpeg-decode.c
	plugin-main.cpp)

add_library(phone-camera MODULE
	${phone-camera_SOURCES}
	${phone-camera_HEADERS})

foreach(lib ${LIB_EVENT_LIBRARIES})
    list (APPEND RELEASE_LINK_LIST optimized ${lib})
endforeach()

foreach(lib ${LIB_EVENT_DEBUG_LIBRARIES})
    list (APPEND DEBUG_LINK_LIST debug ${lib})
endforeach()

target_link_libraries(phone-camera
	${FFMPEG_LIBRARIES}
	${RELEASE_LINK_LIST}
	${DEBUG_LINK_LIST}
	libobs
	Qt5::Gui
	Qt5::Widgets
	Qt5::Network
	Ws2_32
	)

set_target_properties(phone-camera PROPERTIES FOLDER "plugins/phone-camera")

install_obs_plugin(phone-camera)

add_subdirectory(device-helper)
add_subdirectory(mobile-media-backend)
