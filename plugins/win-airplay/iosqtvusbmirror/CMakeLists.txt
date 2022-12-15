project(ios-qtvusb-mirror)

if(APPLE)
	set(_bit_suffix "")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "64bit/")
	set(_lib_bit_suffix "x64")
else()
	set(_bit_suffix "32bit/")
	set(_lib_bit_suffix "x86")
endif()

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(PLIST REQUIRED)
find_package(LibUSB REQUIRED)
find_package(OpenSSL REQUIRED)
set(CMAKE_AUTOMOC TRUE)

include_directories(${PROJECT_SOURCE_DIR}/libwdi/include)
include_directories(${PROJECT_SOURCE_DIR}/mirror/third-party/include)
include_directories(${PROJECT_SOURCE_DIR}/mirror)
include_directories(SYSTEM ${PLIST_INCLUDE_DIR})
 
add_definitions(-DHAVE_OPENSSL)

file(GLOB_RECURSE iosusbmirror_SRC ${PROJECT_SOURCE_DIR}/mirror *.c)
file(GLOB_RECURSE iosusbmirror_HEADER ${PROJECT_SOURCE_DIR}/mirror *.h)

set(ios-qtvusb-mirror_SRC
	AppleDeviceManager.cpp
	MirrorManager.cpp
	DriverHelper.cpp
	InformationWidget.cpp
	main.cpp
)

set(ios-qtvusb-mirror_HEADER
	AppleDeviceManager.h
	MirrorManager.h
	DriverHelper.h
	InformationWidget.h
)
 
add_executable(ios-qtvusb-mirror
	${iosusbmirror_SRC}
	${iosusbmirror_HEADER}
	${ios-qtvusb-mirror_SRC}
	${ios-qtvusb-mirror_HEADER})

target_link_libraries(ios-qtvusb-mirror
	${PLIST_LIBRARIES}
	${OpenSSL_LIBRARIES}
	${LIB_USB_LIBRARIES}
	mirror-ipc
	libobs
	Qt5::Widgets
	Qt5::Network
	debug ${PROJECT_SOURCE_DIR}/libwdi/lib/${_lib_bit_suffix}/libwdid.lib
	optimized ${PROJECT_SOURCE_DIR}/libwdi/lib/${_lib_bit_suffix}/libwdi.lib
	Setupapi
	Crypt32
	Ws2_32
	w32-pthreads
	ntdll)
set_target_properties(ios-qtvusb-mirror PROPERTIES FOLDER "plugins/win-airplay")

install_obs_core(ios-qtvusb-mirror)
