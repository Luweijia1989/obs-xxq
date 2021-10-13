project(android-aoa-server)

find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTOMOC TRUE)

include_directories(
	${PROJECT_SOURCE_DIR}/libwdi/include
	${PROJECT_SOURCE_DIR}/libusb/include
)

set(android-aoa-server_SOURCES
	../ipc.c
	AOADeviceManager.cpp
	DriverHelper.cpp
	InformationWidget.cpp
	main.cpp
)
			 
set(android-aoa-server_HEADERS
	../ipc.h
	AOADeviceManager.h
	DriverHelper.h
	InformationWidget.h
)

add_executable(android-aoa-server
	${android-aoa-server_SOURCES}
	${android-aoa-server_HEADERS}
	)

target_link_libraries(android-aoa-server
	libobs
	Qt5::Widgets
	debug ${PROJECT_SOURCE_DIR}/libwdi/lib/libwdid.lib
	optimized ${PROJECT_SOURCE_DIR}/libwdi/lib/libwdi.lib
	debug ${PROJECT_SOURCE_DIR}/libusb/lib/libusb-1.0-d.lib
	optimized ${PROJECT_SOURCE_DIR}/libusb/lib/libusb-1.0.lib
	Setupapi
	ntdll)

install_obs_core(android-aoa-server)
