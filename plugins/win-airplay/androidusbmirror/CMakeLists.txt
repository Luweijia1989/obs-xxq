project(android-usb-mirror)

file(GLOB_RECURSE androidusbmirror_SRC ${PROJECT_SOURCE_DIR} *.c *.cpp util/*.c util/*.cpp)
file(GLOB_RECURSE androidusbmirror_HEADER ${PROJECT_SOURCE_DIR} *.h *.hpp util/*.h util/*.hpp)

include_directories(
	${PROJECT_SOURCE_DIR}/../
)


set(androidusbmirror_extra_SOURCES
	../ipc.c
)
			 
set(androidusbmirror_extra_HEADERS
	../ipc.h
)

add_executable(android-usb-mirror
	${androidusbmirror_SRC} 
	${androidusbmirror_HEADER}
	${androidusbmirror_extra_SOURCES} 
	${androidusbmirror_extra_HEADERS})

target_link_libraries(android-usb-mirror
	libobs
	Ws2_32
	w32-pthreads)
	 
install_obs_core(android-usb-mirror)
