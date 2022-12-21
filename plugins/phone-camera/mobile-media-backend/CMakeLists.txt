project(mobile-media-backend)

find_package(PLIST REQUIRED)
find_package(LibUSB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Libimobiledevice REQUIRED)
find_package(Libusbmuxd REQUIRED)

find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)

set(CMAKE_AUTOMOC TRUE)

include_directories(SYSTEM ${PLIST_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/ios/screenmirror)
include_directories(${PROJECT_SOURCE_DIR}/ios/usbmuxd)

add_definitions("-DHAVE_LIBIMOBILEDEVICE")
add_definitions("-DHAVE_ENUM_IDEVICE_CONNECTION_TYPE")
add_definitions("-DWIN32_LEAN_AND_MEAN")

file(GLOB usbmuxd_SRC ${PROJECT_SOURCE_DIR}/ios/usbmuxd/*.c)
file(GLOB usbmuxd_HEADER ${PROJECT_SOURCE_DIR}/ios/usbmuxd/*.h)

file(GLOB screenmirror_SRC ${PROJECT_SOURCE_DIR}/ios/screenmirror/*.c)
file(GLOB screenmirror_HEADER ${PROJECT_SOURCE_DIR}/ios/screenmirror/*.h)

set(mobile-media-backend_HEADERS
	../common.h
	../xxq-tcp.h
	application.h
	usb-helper.h
	driver-helper.h
	media-task.h
	media-source.h
	ios-camera.h
	ios-screen-mirror-task-thread.h
	android-camera.h)

set(mobile-media-backend_SOURCES
	main.cpp
	../xxq-tcp.cpp
	application.cpp
	usb-helper.cpp
	driver-helper.cpp
	media-source.cpp
	ios-camera.cpp
	ios-screen-mirror-task-thread.cpp
	android-camera.cpp)

add_executable(mobile-media-backend
	${mobile-media-backend_SOURCES}
	${mobile-media-backend_HEADERS}
	${usbmuxd_SRC}
	${usbmuxd_HEADER}
	${screenmirror_SRC}
	${screenmirror_HEADER})

foreach(lib ${PLIST_LIBRARIES} ${OpenSSL_LIBRARIES} ${FDKAAC_LIBRARIES} ${IMOBILEDEVICE_LIBRARIES} ${LIBUSBMUXD_LIBRARIES} ${LIB_USB_LIBRARIES})
    list (APPEND RELEASE_LINK_LIST optimized ${lib})
endforeach()

foreach(lib ${PLIST_DEBUG_LIBRARIES} ${OpenSSL_DEBUG_LIBRARIES} ${FDKAAC_DEBUG_LIBRARIES} ${IMOBILEDEVICE_DEBUG_LIBRARIES} ${LIBUSBMUXD_DEBUG_LIBRARIES} ${LIB_USB_DEBUG_LIBRARIES})
    list (APPEND DEBUG_LINK_LIST debug ${lib})
endforeach()

target_link_libraries(mobile-media-backend
	${RELEASE_LINK_LIST}
	${DEBUG_LINK_LIST}
	${FFMPEG_LIBRARIES}
	libobs
	w32-pthreads
	Setupapi
	Qt5::Gui
	Qt5::Widgets
	Qt5::Network
	Winusb
	Ws2_32
	)

install_obs_core(mobile-media-backend)
