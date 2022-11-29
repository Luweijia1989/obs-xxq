project(android-aoa-server)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "x64")
else()
	set(_bit_suffix "x86")
endif()

find_package(Qt5Widgets REQUIRED)
find_package(LibUSB REQUIRED)
set(CMAKE_AUTOMOC TRUE)

include_directories(
	${PROJECT_SOURCE_DIR}/libwdi/include
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

foreach(lib ${LIB_USB_LIBRARIES})
    list (APPEND RELEASE_LINK_LIST optimized ${lib})
endforeach()

foreach(lib ${LIB_USB_DEBUG_LIBRARIES})
    list (APPEND DEBUG_LINK_LIST debug ${lib})
endforeach()

target_link_libraries(android-aoa-server
	${LIB_USB_LIBRARIES}
	${DEBUG_LINK_LIST}
	mirror-ipc
	libobs
	Qt5::Widgets
	debug ${PROJECT_SOURCE_DIR}/libwdi/lib/${_bit_suffix}/libwdid.lib
	optimized ${PROJECT_SOURCE_DIR}/libwdi/lib/${_bit_suffix}/libwdi.lib
	Setupapi
	ntdll)

install_obs_core(android-aoa-server)
