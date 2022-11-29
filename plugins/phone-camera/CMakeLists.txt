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

foreach(lib ${PLIST_LIBRARIES} ${OpenSSL_LIBRARIES} ${FDKAAC_LIBRARIES} ${IMOBILEDEVICE_LIBRARIES} ${LIBUSBMUXD_LIBRARIES} ${LIB_USB_LIBRARIES})
    list (APPEND RELEASE_LINK_LIST optimized ${lib})
endforeach()

foreach(lib ${PLIST_DEBUG_LIBRARIES} ${OpenSSL_DEBUG_LIBRARIES} ${FDKAAC_DEBUG_LIBRARIES} ${IMOBILEDEVICE_DEBUG_LIBRARIES} ${LIBUSBMUXD_DEBUG_LIBRARIES} ${LIB_USB_DEBUG_LIBRARIES})
    list (APPEND DEBUG_LINK_LIST debug ${lib})
endforeach()

target_link_libraries(phone-camera
	${RELEASE_LINK_LIST}
	${DEBUG_LINK_LIST}
	Setupapi
	w32-pthreads
	libobs
	Qt5::Gui
	Qt5::Widgets
	Winusb
	)

install_obs_plugin(phone-camera)

add_subdirectory(driver-installer)
