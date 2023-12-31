project(media-playback)

find_package(FFmpeg REQUIRED
	COMPONENTS avcodec avdevice avutil avformat)

include_directories(
	${CMAKE_SOURCE_DIR}/libobs
	${FFMPEG_INCLUDE_DIRS}
	)

set(media-playback_HEADERS
	media-playback/closest-format.h
	media-playback/decode.h
	media-playback/media.h
	)
set(media-playback_SOURCES
	media-playback/decode.c
	media-playback/media.c
	)

add_library(media-playback STATIC
	${media-playback_HEADERS}
	${media-playback_SOURCES}
	)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)64le")
	target_compile_options(media-playback
			PUBLIC
			-mvsx)
	add_compile_definitions(NO_WARN_X86_INTRINSICS)
elseif(NOT MSVC)
	target_compile_options(media-playback
		PUBLIC
			-mmmx
			-msse
			-msse2)
endif()

target_include_directories(media-playback
	PUBLIC .
	)

if(NOT MSVC)
        if(NOT MINGW)
                target_compile_options(media-playback PRIVATE -fPIC)
        endif()
endif()

target_link_libraries(media-playback
	${FFMPEG_LIBRARIES}
	)
set_target_properties(media-playback PROPERTIES FOLDER "deps")