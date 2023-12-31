project(libobs-opengl)


find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

add_definitions(-DLIBOBS_EXPORTS)

if(WIN32)
	set(libobs-opengl_PLATFORM_SOURCES
		gl-windows.c)
elseif(APPLE)
	set(libobs-opengl_PLATFORM_SOURCES
		gl-cocoa.m)

	set_source_files_properties(${libobs-opengl_PLATFORM_SOURCES}
		PROPERTIES
			LANGUAGE C)

	find_library(COCOA Cocoa)
	include_directories(${COCOA})
	mark_as_advanced(COCOA)

	find_library(IOSURF IOSurface)
	include_directories(${IOSURF})
	mark_as_advanced(${IOSURF})

	set(libobs-opengl_PLATFORM_DEPS
		${COCOA}
		${IOSURF}
		${OPENGL_gl_LIBRARY})
else() #This needs to change to be more specific to get ready for Wayland
	find_package(XCB COMPONENTS XCB REQUIRED)
	find_package(X11_XCB REQUIRED)

	include_directories(
		${XCB_INCLUDE_DIRS}
		${X11_XCB_INCLUDE_DIRS})

	add_definitions(
		${XCB_DEFINITIONS}
		${X11_XCB_DEFINITIONS})

	set(libobs-opengl_PLATFORM_DEPS
		${XCB_LIBRARIES}
		${X11_XCB_LIBRARIES})

	set(libobs-opengl_PLATFORM_SOURCES
		gl-x11.c)
endif()

set(libobs-opengl_SOURCES
	${libobs-opengl_PLATFORM_SOURCES}
	gl-helpers.c
	gl-indexbuffer.c
	gl-shader.c
	gl-shaderparser.c
	gl-stagesurf.c
	gl-subsystem.c
	gl-texture2d.c
	gl-texturecube.c
	gl-vertexbuffer.c
	gl-zstencil.c)

set(libobs-opengl_HEADERS
	gl-helpers.h
	gl-shaderparser.h
	gl-subsystem.h)

if(WIN32 OR APPLE)
	add_library(libobs-opengl MODULE
		${libobs-opengl_SOURCES}
		${libobs-opengl_HEADERS})
else()
	add_library(libobs-opengl SHARED
		${libobs-opengl_SOURCES}
		${libobs-opengl_HEADERS})
endif()

if(WIN32 OR APPLE)
set_target_properties(libobs-opengl
	PROPERTIES
		FOLDER "core"
		OUTPUT_NAME libobs-opengl
		PREFIX "")
else()
set_target_properties(libobs-opengl
	PROPERTIES
		FOLDER "core"
		OUTPUT_NAME obs-opengl
		VERSION 0.0
		SOVERSION 0
		)
endif()

target_link_libraries(libobs-opengl
	libobs
	glad
	${libobs-opengl_PLATFORM_DEPS})

install_obs_core(libobs-opengl)
