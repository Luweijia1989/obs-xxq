project(rtmp-server)

if(APPLE)
	set(_bit_suffix "")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "64bit/")
else()
	set(_bit_suffix "32bit/")
endif()

add_definitions(-D__x86_64__)
add_definitions(-D_MBCS)

file(GLOB_RECURSE rtmp-server_SRC ${PROJECT_SOURCE_DIR} 
	src/app/*.cpp 
	src/core/*.cpp 
	src/kernel/*.cpp 
	src/main/*.cpp 
	src/protocol/*.cpp 
	win/src/*.cpp
	win/src/*.c
)
 
file(GLOB_RECURSE rtmp-server_HEADER ${PROJECT_SOURCE_DIR} 
	src/app/*.hpp 
	src/core/*.hpp 
	src/kernel/*.hpp 
	src/protocol/*.hpp 
	win/src/*.h
	win/inc/*.h
	win/inc/*.hpp
	win/inc/arpa/*.h
	win/inc/netinet/*.h
	win/inc/sys/*.h
)


include_directories(
	${PROJECT_SOURCE_DIR}/lib/http-parser-2.1
	${PROJECT_SOURCE_DIR}/lib/openssl/include
	${PROJECT_SOURCE_DIR}/lib/st
	${PROJECT_SOURCE_DIR}/lib/fdk-aac
	${PROJECT_SOURCE_DIR}/src/protocol
	${PROJECT_SOURCE_DIR}/src/kernel
	${PROJECT_SOURCE_DIR}/src/app
	${PROJECT_SOURCE_DIR}/src/core
	${PROJECT_SOURCE_DIR}/src/libs
	${PROJECT_SOURCE_DIR}/src/utest
	${PROJECT_SOURCE_DIR}/win/inc
	${PROJECT_SOURCE_DIR}/../
)


set(rtmp-server_extra_SOURCES
	../ipc.c
)
			 
set(rtmp-server_extra_HEADERS
	../ipc.h
)

add_executable(rtmp-server
	${rtmp-server_SRC} 
	${rtmp-server_HEADER}
	${rtmp-server_extra_SOURCES} 
	${rtmp-server_extra_HEADERS})

target_link_libraries(rtmp-server
	libobs
	Ws2_32
	w32-pthreads
	debug ${PROJECT_SOURCE_DIR}/lib/st.lib
	optimized ${PROJECT_SOURCE_DIR}/lib/stR.lib
	debug ${PROJECT_SOURCE_DIR}/lib/fdk-aac.lib
	optimized ${PROJECT_SOURCE_DIR}/lib/fdk-aacR.lib
	${PROJECT_SOURCE_DIR}/lib/http-parser.lib
	${PROJECT_SOURCE_DIR}/lib/libeay32.lib
	${PROJECT_SOURCE_DIR}/lib/ssleay32.lib
	legacy_stdio_definitions
)
	 
install_obs_core(rtmp-server)
