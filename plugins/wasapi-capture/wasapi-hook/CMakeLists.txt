project(wasapi-hook)

if(MSVC)
	add_compile_options("$<IF:$<CONFIG:Debug>,/MTd,/MT>")
endif()

include_directories(${PROJECT_SOURCE_DIR}/minhook)
file(GLOB_RECURSE minihook_SRC ${PROJECT_SOURCE_DIR}/minhook ${PROJECT_SOURCE_DIR}/minhook/hde *.c)
file(GLOB_RECURSE minihook_HEADER ${PROJECT_SOURCE_DIR}/minhook ${PROJECT_SOURCE_DIR}/minhook/hde *.h)

set(wasapi-hook_HEADERS
    circlebuf.h
    wasapi-hook.h
    funchook.h
    wasapi_capture_proxy.h
    wasapi_capturer.h
    ../obfuscate.h
    ../wasapi-hook-info.h)

set(wasapi-hook_SOURCES
    funchook.c
    wasapi-hook.c
    wasapi_capture_proxy.cpp
    wasapi_capturer.cpp
    ../obfuscate.c)

add_library(wasapi-hook MODULE
    ${minihook_SRC}
    ${minihook_HEADER}
    ${wasapi-hook_HEADERS}
    ${wasapi-hook_SOURCES})

target_link_libraries(wasapi-hook
    ipc-util
    psapi)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_output_suffix "64")
else()
    set(_output_suffix "32")
endif()

set_target_properties(wasapi-hook
PROPERTIES
    	OUTPUT_NAME "wasapi-hook${_output_suffix}")

install_obs_datatarget(wasapi-hook "obs-plugins/wasapi-capture")
