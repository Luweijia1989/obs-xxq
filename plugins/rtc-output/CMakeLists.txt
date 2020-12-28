if (NOT WIN32)
	return()
endif()

project(rtc-output)

include_directories(${PROJECT_SOURCE_DIR}/trtc/sdk/include)
include_directories(${PROJECT_SOURCE_DIR})

find_package(Qt5Core REQUIRED)

set(rtc-output_SOURCES
	rtc-output.cpp
	trtc/TRTCCloudCore.cpp
	trtc/DataCenter.cpp
	rtc-param.cpp
	rtc-base.cpp
)
		
		
set(rtc-output_HEADERS
	rtc-output.h
	trtc/TRTCCloudCore.h
	trtc/DataCenter.h
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
	${PROJECT_SOURCE_DIR}/trtc/sdk/lib/liteav.lib)

install_obs_plugin(rtc-output)
