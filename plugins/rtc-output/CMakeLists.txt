if (NOT WIN32)
	return()
endif()

project(rtc-output)

include_directories(${PROJECT_SOURCE_DIR}/trtc/sdk/include)
include_directories(${PROJECT_SOURCE_DIR})


set(rtc-output_SOURCES
	rtc-output.cpp
	trtc/trtc.cpp
)
		
		
set(rtc-output_HEADERS
	rtc-output.h
	trtc/trtc.h
)

add_library(rtc-output MODULE
	${rtc-output_SOURCES}
	${rtc-output_HEADERS})
target_link_libraries(rtc-output
	libobs
	w32-pthreads
	${PROJECT_SOURCE_DIR}/trtc/sdk/lib/liteav.lib)

install_obs_plugin(rtc-output)
