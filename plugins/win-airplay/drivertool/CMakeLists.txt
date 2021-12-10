project(ios-qtvusb-mirror)

find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTOMOC TRUE)

include_directories(
	${PROJECT_SOURCE_DIR}/libwdi/include
)

set(drivertool_SRC
	AppleDeviceManager.cpp
	DriverHelper.cpp
	InformationWidget.cpp
	main.cpp
)
			 
set(drivertool_HEADER
	AppleDeviceManager.h
	DriverHelper.h
	InformationWidget.h
)

set(drivertool_extra_SOURCES
	../ipc.c
)
			 
set(drivertool_extra_HEADERS
	../ipc.h
)
 
add_executable(ios-qtvusb-mirror
	${drivertool_SRC}
	${drivertool_HEADER}
	${drivertool_extra_SOURCES}
	${drivertool_extra_HEADERS})

target_link_libraries(ios-qtvusb-mirror
	libobs
	Qt5::Widgets
	debug ${PROJECT_SOURCE_DIR}/libwdi/lib/libwdid.lib
	optimized ${PROJECT_SOURCE_DIR}/libwdi/lib/libwdi.lib
	Setupapi)

install_obs_core(ios-qtvusb-mirror)
