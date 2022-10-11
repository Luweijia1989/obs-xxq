project(wasapi-capture)

set(wasapi-capture_HEADERS
	nt-stuff.h
	hook-helpers.h
	wasapi-hook-info.h
	app-helpers.h
	obfuscate.h
	audio-channel.h
	wasapi-capture.h)

set(wasapi-capture_SOURCES
	audio-channel.c
	wasapi-capture.c
	app-helpers.c
	obfuscate.c
	plugin-main.c)

add_library(wasapi-capture MODULE
	${wasapi-capture_SOURCES}
	${wasapi-capture_HEADERS})
target_link_libraries(wasapi-capture
	libobs
	ipc-util
	psapi
	w32-pthreads)
set_target_properties(wasapi-capture PROPERTIES FOLDER "plugins/wasapi-capture")

install_obs_plugin(wasapi-capture data)

add_subdirectory(wasapi-hook)
add_subdirectory(get-wasapi-offsets)
