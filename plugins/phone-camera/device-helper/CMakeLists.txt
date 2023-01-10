project(device-helper)

find_package(Qt5Widgets REQUIRED)
find_package(LibUSB REQUIRED)
set(CMAKE_AUTOMOC TRUE)

include_directories(${PROJECT_SOURCE_DIR}/libwdi/include)

set(device-helper_SRC
	main.cpp
	DriverHelper.cpp
	InformationWidget.cpp
)

set(device-helper_HEADER
	DriverHelper.h
	InformationWidget.h
)
 
add_executable(device-helper
	${device-helper_SRC}
	${device-helper_HEADER}
	)

foreach(lib ${LIB_USB_LIBRARIES})
    list (APPEND RELEASE_LINK_LIST optimized ${lib})
endforeach()

foreach(lib ${LIB_USB_DEBUG_LIBRARIES})
    list (APPEND DEBUG_LINK_LIST debug ${lib})
endforeach()

target_link_libraries(device-helper
	${RELEASE_LINK_LIST}
	${DEBUG_LINK_LIST}
	Setupapi
	${PROJECT_SOURCE_DIR}/libwdi/lib/libwdi.lib
	Qt5::Widgets
	)

set_target_properties(device-helper PROPERTIES FOLDER "plugins/phone-camera")

install_obs_core(device-helper)
