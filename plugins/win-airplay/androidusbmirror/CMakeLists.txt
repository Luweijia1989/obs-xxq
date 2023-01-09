project(android-usb-mirror)

if(APPLE)
	set(_bit_suffix "")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "64bit/")
else()
	set(_bit_suffix "32bit/")
endif()

file(GLOB_RECURSE androidusbmirror_SRC ${PROJECT_SOURCE_DIR} *.c *.cpp util/*.c util/*.cpp)
file(GLOB_RECURSE androidusbmirror_HEADER ${PROJECT_SOURCE_DIR} *.h *.hpp util/*.h util/*.hpp)

include_directories(
	${PROJECT_SOURCE_DIR}/../
)

add_executable(android-usb-mirror
	${androidusbmirror_SRC} 
	${androidusbmirror_HEADER})

target_link_libraries(android-usb-mirror
	mirror-ipc
	libobs
	Ws2_32
	w32-pthreads)
set_target_properties(android-usb-mirror PROPERTIES FOLDER "plugins/win-airplay")

install_obs_core(android-usb-mirror)

function(install_dependency target)
add_custom_command(TARGET android-usb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/deps/${target}"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}${target}"
		VERBATIM)
add_custom_command(TARGET android-usb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/deps/${target}"
			"$<TARGET_FILE_DIR:android-usb-mirror>/${target}"
		VERBATIM)
endfunction()

install_dependency("adb.exe")
install_dependency("AdbWinApi.dll")
install_dependency("AdbWinUsbApi.dll")
install_dependency("scrcpy-server")
