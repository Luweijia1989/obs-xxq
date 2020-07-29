project(driver-tool)

file(GLOB_RECURSE drivertool_SRC ${PROJECT_SOURCE_DIR} *.c *.cpp)
file(GLOB_RECURSE drivertool_HEADER ${PROJECT_SOURCE_DIR} *.h *.hpp)

find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTOMOC TRUE)

include_directories(
		${PROJECT_SOURCE_DIR}/libwdi/include
        )

add_executable(driver-tool
	${drivertool_SRC}
	${drivertool_HEADER})

target_link_libraries(driver-tool
	Qt5::Widgets
	debug ${PROJECT_SOURCE_DIR}/libwdi/lib/libwdid.lib
	optimized ${PROJECT_SOURCE_DIR}/libwdi/lib/libwdi.lib
	Setupapi)

install_obs_core(driver-tool)
