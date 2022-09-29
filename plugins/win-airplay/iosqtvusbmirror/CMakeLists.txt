project(ios-qtvusb-mirror)

if(APPLE)
	set(_bit_suffix "")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "64bit/")
	set(_lib_bit_suffix "x64")
else()
	set(_bit_suffix "32bit/")
	set(_lib_bit_suffix "x86")
endif()

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)
set(CMAKE_AUTOMOC TRUE)

include_directories(${PROJECT_SOURCE_DIR}/libwdi/include)
include_directories(${PROJECT_SOURCE_DIR}/mirror/third-party/include)
include_directories(${PROJECT_SOURCE_DIR}/mirror)
include_directories(${PROJECT_SOURCE_DIR}/openssl/include)
 
add_definitions(-DHAVE_OPENSSL)

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
 
add_executable(ios-qtvusb-mirror
	${iosusbmirror_SRC}
	${iosusbmirror_HEADER}
	${ios-qtvusb-mirror_SRC}
	${ios-qtvusb-mirror_HEADER})

target_link_libraries(ios-qtvusb-mirror
	mirror-ipc
	libobs
	Qt5::Widgets
	Qt5::Network
	debug ${PROJECT_SOURCE_DIR}/libwdi/lib/${_lib_bit_suffix}/libwdid.lib
	optimized ${PROJECT_SOURCE_DIR}/libwdi/lib/${_lib_bit_suffix}/libwdi.lib
	debug ${PROJECT_SOURCE_DIR}/mirror/third-party/lib/${_lib_bit_suffix}/debug/plist.lib
	debug ${PROJECT_SOURCE_DIR}/mirror/third-party/lib/${_lib_bit_suffix}/debug/libusb0.lib
	optimized ${PROJECT_SOURCE_DIR}/mirror/third-party/lib/${_lib_bit_suffix}/relwithdebinfo/plist.lib
	optimized ${PROJECT_SOURCE_DIR}/mirror/third-party/lib/${_lib_bit_suffix}/relwithdebinfo/libusb0.lib
	debug ${PROJECT_SOURCE_DIR}/openssl/lib/${_lib_bit_suffix}/debug/libcrypto.lib
	debug ${PROJECT_SOURCE_DIR}/openssl/lib/${_lib_bit_suffix}/debug/libssl.lib
	optimized ${PROJECT_SOURCE_DIR}/openssl/lib/${_lib_bit_suffix}/release/libcrypto.lib
	optimized ${PROJECT_SOURCE_DIR}/openssl/lib/${_lib_bit_suffix}/release/libssl.lib
	Setupapi
	Crypt32
	Ws2_32
	w32-pthreads
	ntdll)
set_target_properties(ios-qtvusb-mirror PROPERTIES FOLDER "plugins/win-airplay")

install_obs_core(ios-qtvusb-mirror)

function(install_dependency target)
add_custom_command(TARGET ios-qtvusb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/mirror/third-party/bin/${_lib_bit_suffix}/$<CONFIGURATION>/${target}.pdb"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}${target}.pdb"
		VERBATIM)
add_custom_command(TARGET ios-qtvusb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/mirror/third-party/bin/${_lib_bit_suffix}/$<CONFIGURATION>/${target}.dll"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}${target}.dll"
		VERBATIM)
add_custom_command(TARGET ios-qtvusb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/mirror/third-party/bin/${_lib_bit_suffix}/$<CONFIGURATION>/${target}.pdb"
			"$<TARGET_FILE_DIR:ios-qtvusb-mirror>/${target}.pdb"
		VERBATIM)
add_custom_command(TARGET ios-qtvusb-mirror POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/mirror/third-party/bin/${_lib_bit_suffix}/$<CONFIGURATION>/${target}.dll"
			"$<TARGET_FILE_DIR:ios-qtvusb-mirror>/${target}.dll"
		VERBATIM)
endfunction()
 
install_dependency("plist") 
