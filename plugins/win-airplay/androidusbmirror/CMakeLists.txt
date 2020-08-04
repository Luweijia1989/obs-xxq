project(android-usb-mirror)

file(GLOB_RECURSE androidusbmirror_SRC ${PROJECT_SOURCE_DIR} *.c *.cpp util/*.c util/*.cpp)
file(GLOB_RECURSE androidusbmirror_HEADER ${PROJECT_SOURCE_DIR} *.h *.hpp util/*.h util/*.hpp)

add_executable(android-usb-mirror
	${androidusbmirror_SRC}
	${androidusbmirror_HEADER})

target_link_libraries(android-usb-mirror
	Ws2_32 
	w32-pthreads)
	 
install_obs_core(android-usb-mirror)
