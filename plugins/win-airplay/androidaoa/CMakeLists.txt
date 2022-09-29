project(android-aoa-server)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "x64")
else()
	set(_bit_suffix "x86")
endif()

find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTOMOC TRUE)

include_directories(
	${PROJECT_SOURCE_DIR}/libwdi/include
	${PROJECT_SOURCE_DIR}/libusb/include
)

set(android-aoa-server_SOURCES
	AOADeviceManager.cpp
	DriverHelper.cpp
	InformationWidget.cpp
	main.cpp
)
			 
set(android-aoa-server_HEADERS
	AOADeviceManager.h
	DriverHelper.h
	InformationWidget.h
)

add_executable(android-aoa-server
	${android-aoa-server_SOURCES}
	${android-aoa-server_HEADERS}
	)

target_link_libraries(android-aoa-server
	mirror-ipc
	libobs
	Qt5::Widgets
	debug ${PROJECT_SOURCE_DIR}/libwdi/lib/${_bit_suffix}/libwdid.lib
	optimized ${PROJECT_SOURCE_DIR}/libwdi/lib/${_bit_suffix}/libwdi.lib
	debug ${PROJECT_SOURCE_DIR}/libusb/lib/${_bit_suffix}/libusb-1.0-d.lib
	optimized ${PROJECT_SOURCE_DIR}/libusb/lib/${_bit_suffix}/libusb-1.0.lib
	Setupapi
	ntdll)
set_target_properties(android-aoa-server PROPERTIES FOLDER "plugins/win-airplay")

install_obs_core(android-aoa-server)
