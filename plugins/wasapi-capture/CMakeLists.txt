project(wasapi-capture)

set(wasapi-capture_HEADERS
	nt-stuff.h
	hook-helpers.h
	wasapi-hook-info.h
	window-helpers.h
	app-helpers.h
	obfuscate.h)

set(wasapi-capture_SOURCES
	wasapi-capture.c
	window-helpers.c
	app-helpers.c
	obfuscate.c
	plugin-main.c)

add_library(wasapi-capture MODULE
	${wasapi-capture_SOURCES}
	${wasapi-capture_HEADERS})
target_link_libraries(wasapi-capture
	libobs
	ipc-util
	psapi)
set_target_properties(wasapi-capture PROPERTIES FOLDER "plugins/wasapi-capture")

install_obs_plugin(wasapi-capture data)

add_subdirectory(wasapi-hook)
