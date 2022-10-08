project(wasapi-hook)

find_package(Detours REQUIRED)

include_directories(${DETOURS_INCLUDE_DIR})

if(MSVC)
	add_compile_options("$<IF:$<CONFIG:Debug>,/MTd,/MT>")
endif()

set(wasapi-hook_HEADERS
    circlebuf.h
    wasapi-hook.h
    wasapi_capturer.h
    ../obfuscate.h
    ../wasapi-hook-info.h)

set(wasapi-hook_SOURCES
    wasapi-hook.c
    wasapi_capturer.cpp
    ../obfuscate.c)

add_library(wasapi-hook MODULE
    ${minihook_SRC}
    ${minihook_HEADER}
    ${wasapi-hook_HEADERS}
    ${wasapi-hook_SOURCES})

target_link_libraries(wasapi-hook
    ipc-util
    psapi
    ${DETOURS_LIB})

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_output_suffix "64")
else()
    set(_output_suffix "32")
endif()

set_target_properties(wasapi-hook
PROPERTIES
        FOLDER "plugins/wasapi-capture"
    	OUTPUT_NAME "wasapi-hook${_output_suffix}")

install_obs_datatarget(wasapi-hook "obs-plugins/wasapi-capture")
