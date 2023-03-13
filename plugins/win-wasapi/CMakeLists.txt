project(win-wasapi)

find_package(Libspeexdsp REQUIRED)
include_directories(${LIBSPEEXDSP_INCLUDE_DIRS})

set(win-wasapi_HEADERS
	enum-wasapi.hpp
	xxq-aec.h)

set(win-wasapi_SOURCES
	xxq-aec.cpp
	win-wasapi.cpp
	pure-audio-source.cpp
	enum-wasapi.cpp
	plugin-main.cpp)

add_library(win-wasapi MODULE
	${win-wasapi_SOURCES}
	${win-wasapi_HEADERS})
target_link_libraries(win-wasapi
	libobs
	${LIBSPEEXDSP_LIBRARIES})
set_target_properties(win-wasapi PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(win-wasapi data)
