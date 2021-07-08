if (NOT WIN32)
	return()
endif()

project(obs-text)

if(WIN32)
	set(obs-text_PLATFORM_SOURCES
		gdiplus/obs-text.cpp
		gdiplus/SlideTextSource.cpp
		gdiplus/ScrollTextSource.cpp
		gdiplus/GauseBlur.cpp
		)
	set(obs-text_PLATFORM_DEPS
		gdiplus)
		
		
	set(obs-text_PLATFORM_HEADERS
	gdiplus/SlideTextSource.h
	gdiplus/ScrollTextSource.h
	gdiplus/Common.h
	gdiplus/GauseBlur.h
	)

endif()

add_library(obs-text MODULE
	${obs-text_PLATFORM_SOURCES}
	${obs-text_PLATFORM_HEADERS})
target_link_libraries(obs-text
	libobs
	${obs-text_PLATFORM_DEPS})

install_obs_plugin_with_data(obs-text data)
