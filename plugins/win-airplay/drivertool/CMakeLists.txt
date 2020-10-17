project(driver-tool)

file(GLOB_RECURSE drivertool_SRC ${PROJECT_SOURCE_DIR} *.c *.cpp)
file(GLOB_RECURSE drivertool_HEADER ${PROJECT_SOURCE_DIR} *.h *.hpp)

find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTOMOC TRUE)

include_directories(
	${PROJECT_SOURCE_DIR}/libwdi/include
)

set(drivertool_extra_SOURCES
	../ipc.c
)
			 
set(drivertool_extra_HEADERS
	../ipc.h
)

add_executable(driver-tool
	${drivertool_SRC}
	${drivertool_HEADER}
	${drivertool_extra_SOURCES}
	${drivertool_extra_HEADERS})

target_link_libraries(driver-tool
	libobs
	Qt5::Widgets
	debug ${PROJECT_SOURCE_DIR}/libwdi/lib/libwdid.lib
	optimized ${PROJECT_SOURCE_DIR}/libwdi/lib/libwdi.lib
	Setupapi)

install_obs_core(driver-tool)
