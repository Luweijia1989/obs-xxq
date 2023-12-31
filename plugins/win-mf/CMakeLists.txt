project(win-mf)

set(ENABLE_WINMF FALSE CACHE BOOL "Enables the now deprecated win-mf plugin")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/mf-config.hpp.in"
	"${CMAKE_CURRENT_BINARY_DIR}/mf-config.hpp")

set(win-mf_config_HEADERS
	"${CMAKE_CURRENT_BINARY_DIR}/mf-config.hpp")

if(ENABLE_WINMF)
	set(win-mf_SOURCES
		mf-plugin.cpp
		mf-aac.cpp
		mf-aac-encoder.cpp
		mf-common.cpp
		mf-encoder-descriptor.cpp
		mf-h264.cpp
		mf-h264-encoder.cpp)

	set(win-mf_HEADERS
		mf-common.hpp
		mf-encoder-descriptor.hpp
		mf-aac-encoder.hpp
		mf-h264-encoder.hpp)

	set(win-mf_DEPS
		d3d9
		dxva2
		uuid
		mfplat
		mfuuid
		mf
		wmcodecdspuuid)
else()
	set(win-mf_SOURCES
		mf-plugin.cpp)
endif()

add_library(win-mf MODULE
	${win-mf_config_HEADERS}
	${win-mf_SOURCES}
	${win-mf_HEADERS})

target_link_libraries(win-mf
	${win-mf_DEPS}
	libobs)
set_target_properties(win-mf PROPERTIES FOLDER "plugins")

target_include_directories(win-mf
	PUBLIC
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")

if(ENABLE_WINMF)
	install_obs_plugin_with_data(win-mf data)
else()
	install_obs_plugin(win-mf)
endif()
