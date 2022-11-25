project(driver-installer)

find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTOMOC TRUE)

include_directories(${PROJECT_SOURCE_DIR}/libwdi/include)

set(driver-installer_SRC
	main.cpp
	DriverHelper.cpp
	InformationWidget.cpp
)

set(driver-installer_HEADER
	DriverHelper.h
	InformationWidget.h
)
 
add_executable(driver-installer
	${driver-installer_SRC}
	${driver-installer_HEADER}
	)

target_link_libraries(driver-installer
	Setupapi
	${PROJECT_SOURCE_DIR}/libwdi/lib/libwdi.lib
	Qt5::Widgets
	)

install_obs_core(driver-installer)
