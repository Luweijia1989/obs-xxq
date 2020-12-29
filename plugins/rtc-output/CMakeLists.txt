if (NOT WIN32)
	return()
endif()

project(rtc-output)

set(CMAKE_AUTOMOC TRUE)

if(APPLE)
	set(_bit_suffix "")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "64bit/")
else()
	set(_bit_suffix "32bit/")
endif()

include_directories(${PROJECT_SOURCE_DIR}/trtc/sdk/include)
include_directories(${PROJECT_SOURCE_DIR}/trtc/sdk/include/TRTC)
include_directories(${PROJECT_SOURCE_DIR})

find_package(Qt5Core REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(SYSTEM ${ZLIB_INCLUDE_DIR})

set(rtc-output_SOURCES
	rtc-output.cpp
	trtc/TRTCCloudCore.cpp
	trtc/DataCenter.cpp
	trtc/GenerateTestUserSig.cpp
	rtc-param.cpp
	rtc-base.cpp
)
		
		
set(rtc-output_HEADERS
	rtc-output.h
	trtc/TRTCCloudCore.h
	trtc/DataCenter.h
	trtc/GenerateTestUserSig.h
	rtc-param.h
	rtc-base.h
	rtc-define.h
)

add_library(rtc-output MODULE
	${rtc-output_SOURCES}
	${rtc-output_HEADERS})
target_link_libraries(rtc-output
	libobs
	Qt5::Core
	w32-pthreads
	${ZLIB_LIBRARIES}
	${PROJECT_SOURCE_DIR}/trtc/sdk/lib/liteav.lib)

install_obs_plugin(rtc-output)

function(install_dependency target)
add_custom_command(TARGET rtc-output POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/trtc/sdk/lib/${target}"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}${target}"
		VERBATIM)
add_custom_command(TARGET rtc-output POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/trtc/sdk/lib/${target}"
			"$<TARGET_FILE_DIR:rtc-output>/${target}"
		VERBATIM)
endfunction()

install_dependency("liteav.dll")
