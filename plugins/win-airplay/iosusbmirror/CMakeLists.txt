project(ios-usb-mirror)

set(OBS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/rundir")

if(APPLE)
	set(_bit_suffix "")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "64bit/")
else()
	set(_bit_suffix "32bit/")
endif()

include_directories(
	${PROJECT_SOURCE_DIR}/third-party/include
)

file(GLOB_RECURSE iosusbmirror_SRC ${PROJECT_SOURCE_DIR} *.c)
file(GLOB_RECURSE iosusbmirror_HEADER ${PROJECT_SOURCE_DIR} *.h)

add_definitions( -D_USE_32BIT_TIME_T -DHAVE_LIBIMOBILEDEVICE )

add_executable(ios-usb-mirror
	${iosusbmirror_SRC}
	${iosusbmirror_HEADER})

target_link_libraries(ios-usb-mirror
	debug ${PROJECT_SOURCE_DIR}/third-party/lib/debug/plist.lib
	debug ${PROJECT_SOURCE_DIR}/third-party/lib/debug/libimobiledevice.lib
	debug ${PROJECT_SOURCE_DIR}/third-party/lib/debug/libusb0.lib
	optimized ${PROJECT_SOURCE_DIR}/third-party/lib/relwithdebinfo/plist.lib
	optimized ${PROJECT_SOURCE_DIR}/third-party/lib/relwithdebinfo/libimobiledevice.lib
	optimized ${PROJECT_SOURCE_DIR}/third-party/lib/relwithdebinfo/libusb0.lib
	Ws2_32
	ipc-util
	w32-pthreads)

install_obs_core(ios-usb-mirror)

function(install_dependency target)
add_custom_command(TARGET ios-usb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/third-party/bin/$<CONFIGURATION>/${target}.pdb"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}${target}.pdb"
		VERBATIM)
add_custom_command(TARGET ios-usb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/third-party/bin/$<CONFIGURATION>/${target}.dll"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}${target}.dll"
		VERBATIM)
add_custom_command(TARGET ios-usb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/third-party/bin/$<CONFIGURATION>/${target}.pdb"
			"$<TARGET_FILE_DIR:ios-usb-mirror>/${target}.pdb"
		VERBATIM)
add_custom_command(TARGET ios-usb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/third-party/bin/$<CONFIGURATION>/${target}.dll"
			"$<TARGET_FILE_DIR:ios-usb-mirror>/${target}.dll"
		VERBATIM)
endfunction()
 
install_dependency("getopt") 
install_dependency("libimobiledevice")
install_dependency("libusb0")
install_dependency("libusbmuxd") 
install_dependency("plist") 
install_dependency("libcrypto-1_1") 
install_dependency("libssl-1_1") 

