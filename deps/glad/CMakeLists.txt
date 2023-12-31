project(glad)

find_package(OpenGL)

if(NOT WIN32 AND NOT APPLE)
	find_package(X11)
endif()

set(glad_SOURCES
	src/glad.c
	include/glad/glad.h)

if(WIN32)
	set(glad_PLATFORM_SOURCES
		src/glad_wgl.c
		include/glad/glad_wgl.h)
elseif(NOT APPLE)
	set(glad_PLATFORM_SOURCES
		src/glad_glx.c
		include/glad/glad_glx.h)
endif()

set(glad_include_dirs
	PRIVATE ${OPENGL_INCLUDE_DIR})

if (UNIX AND NOT APPLE)
list (APPEND glad_include_dirs
	PRIVATE ${X11_X11_INCLUDE_PATH})
endif()

add_library(glad SHARED
	${glad_SOURCES}
	${glad_PLATFORM_SOURCES})
set_target_properties(glad PROPERTIES
	FOLDER "deps"
	OUTPUT_NAME obsglad
	VERSION "0"
	SOVERSION "0")
target_include_directories(glad
	PUBLIC include
	${glad_include_dirs})
target_compile_definitions(glad
	PRIVATE GLAD_GLAPI_EXPORT_BUILD)

if(NOT MSVC)
	target_compile_options(glad
		PRIVATE -DPIC -fvisibility=hidden)
	if(NOT MINGW)
		target_compile_options(glad PRIVATE -fPIC)
	endif()
endif()

if(NOT WIN32 AND NOT APPLE)
	set(glad_PLATFORM_DEPS
		${X11_X11_LIB})
	# only link to libdl on linux
	if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
		set(glad_PLATFORM_DEPS
			${glad_PLATFORM_DEPS}
			-ldl)
	endif()
endif()

target_link_libraries(glad
	${glad_PLATFORM_DEPS}
	${OPENGL_gl_LIBRARY})

install_obs_core(glad)
