project(ios-qtvusb-mirror)

find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTOMOC TRUE)

include_directories(${PROJECT_SOURCE_DIR}/libwdi/include)
include_directories(${PROJECT_SOURCE_DIR}/mirror/third-party/include)
include_directories(${PROJECT_SOURCE_DIR}/mirror)

add_definitions( -D_USE_32BIT_TIME_T -DHAVE_LIBIMOBILEDEVICE )

file(GLOB_RECURSE iosusbmirror_SRC ${PROJECT_SOURCE_DIR}/mirror *.c)
file(GLOB_RECURSE iosusbmirror_HEADER ${PROJECT_SOURCE_DIR}/mirror *.h)

set(ios-qtvusb-mirror_SRC
	AppleDeviceManager.cpp
	MirrorManager.cpp
	DriverHelper.cpp
	InformationWidget.cpp
	main.cpp
)
	 		 
set(ios-qtvusb-mirror_HEADER
	AppleDeviceManager.h
	MirrorManager.h
	DriverHelper.h
	InformationWidget.h
)

set(ios-qtvusb-mirror_extra_SOURCES
	../ipc.c
)
			 
set(ios-qtvusb-mirror_extra_HEADERS
	../ipc.h
)
 
add_executable(ios-qtvusb-mirror
	${iosusbmirror_SRC}
	${iosusbmirror_HEADER}
	${ios-qtvusb-mirror_SRC}
	${ios-qtvusb-mirror_HEADER}
	${ios-qtvusb-mirror_extra_SOURCES}
	${ios-qtvusb-mirror_extra_HEADERS})

target_link_libraries(ios-qtvusb-mirror
	libobs
	Qt5::Widgets
	debug ${PROJECT_SOURCE_DIR}/libwdi/lib/libwdid.lib
	optimized ${PROJECT_SOURCE_DIR}/libwdi/lib/libwdi.lib
	debug ${PROJECT_SOURCE_DIR}/mirror/third-party/lib/debug/plist.lib
	debug ${PROJECT_SOURCE_DIR}/mirror/third-party/lib/debug/libimobiledevice.lib
	debug ${PROJECT_SOURCE_DIR}/mirror/third-party/lib/debug/libusb0.lib
	optimized ${PROJECT_SOURCE_DIR}/mirror/third-party/lib/relwithdebinfo/plist.lib
	optimized ${PROJECT_SOURCE_DIR}/mirror/third-party/lib/relwithdebinfo/libimobiledevice.lib
	optimized ${PROJECT_SOURCE_DIR}/mirror/third-party/lib/relwithdebinfo/libusb0.lib
	Setupapi
	Ws2_32
	w32-pthreads)

install_obs_core(ios-qtvusb-mirror)

function(install_dependency target)
add_custom_command(TARGET ios-qtvusb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/mirror/third-party/bin/$<CONFIGURATION>/${target}.pdb"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}${target}.pdb"
		VERBATIM)
add_custom_command(TARGET ios-qtvusb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/mirror/third-party/bin/$<CONFIGURATION>/${target}.dll"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}${target}.dll"
		VERBATIM)
add_custom_command(TARGET ios-qtvusb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/mirror/third-party/bin/$<CONFIGURATION>/${target}.pdb"
			"$<TARGET_FILE_DIR:ios-qtvusb-mirror>/${target}.pdb"
		VERBATIM)
add_custom_command(TARGET ios-qtvusb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/mirror/third-party/bin/$<CONFIGURATION>/${target}.dll"
			"$<TARGET_FILE_DIR:ios-qtvusb-mirror>/${target}.dll"
		VERBATIM)
endfunction()
 
install_dependency("getopt") 
install_dependency("libimobiledevice")
install_dependency("libusb0")
install_dependency("libusbmuxd") 
install_dependency("plist") 
install_dependency("libcrypto-1_1") 
install_dependency("libssl-1_1") 
