project(ios-usb-mirror)

file(GLOB_RECURSE iosusbmirror_SRC ${PROJECT_SOURCE_DIR} *.c)
file(GLOB_RECURSE iosusbmirror_HEADER ${PROJECT_SOURCE_DIR} *.h)

add_definitions( -D_USE_32BIT_TIME_T -DHAVE_LIBIMOBILEDEVICE )

add_executable(ios-usb-mirror
	${iosusbmirror_SRC}
	${iosusbmirror_HEADER})

target_link_libraries(ios-usb-mirror
	Ws2_32
	ipc-util)

install_obs_core(ios-usb-mirror)
