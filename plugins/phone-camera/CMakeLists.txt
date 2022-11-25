project(phone-camera)

find_package(PLIST REQUIRED)
find_package(LibUSB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Libimobiledevice REQUIRED)
find_package(Libusbmuxd REQUIRED)

find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)

set(CMAKE_AUTOMOC TRUE)

include_directories(SYSTEM "${PROJECT_SOURCE_DIR}/deps/include")
include_directories(SYSTEM ${PLIST_INCLUDE_DIR})

add_definitions("-DHAVE_LIBIMOBILEDEVICE")
add_definitions("-DHAVE_ENUM_IDEVICE_CONNECTION_TYPE")

file(GLOB usbmuxd_SRC ${PROJECT_SOURCE_DIR}/ios/usbmuxd/*.c)
file(GLOB usbmuxd_HEADER ${PROJECT_SOURCE_DIR}/ios/usbmuxd/*.h)

set(phone-camera_HEADERS
	phone-camera.h
	usb-helper.h)

set(phone-camera_SOURCES
	phone-camera.cpp
	usb-helper.cpp
	plugin-main.cpp)

add_library(phone-camera MODULE
	${phone-camera_SOURCES}
	${phone-camera_HEADERS}
	${usbmuxd_SRC}
	${usbmuxd_HEADER})
target_link_libraries(phone-camera
	Setupapi
	${PLIST_LIBRARIES}
	${OpenSSL_LIBRARIES}
	${LIB_USB_LIBRARIES}
	${IMOBILEDEVICE_LIBRARIES}
	${LIBUSBMUXD_LIBRARIES}
	w32-pthreads
	libobs
	Qt5::Gui
	Qt5::Widgets
	Winusb
	)

install_obs_plugin(phone-camera)

add_subdirectory(driver-installer)
