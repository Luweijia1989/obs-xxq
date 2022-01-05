project(airplay-server)

include_directories(${PROJECT_SOURCE_DIR}/third-party/include)
include_directories(${PROJECT_SOURCE_DIR}/lib)

add_definitions(-DWIN32_LEAN_AND_MEAN -DHAS_DUMMY_RENDERER)

file(GLOB_RECURSE airplay_SRC ${PROJECT_SOURCE_DIR}/lib ${PROJECT_SOURCE_DIR}/llhttp ${PROJECT_SOURCE_DIR}/playfair *.c)
file(GLOB_RECURSE airplay_HEADER ${PROJECT_SOURCE_DIR}/lib ${PROJECT_SOURCE_DIR}/llhttp ${PROJECT_SOURCE_DIR}/playfair *.h)

set(airplay_main_SRC
	rpiplay.cpp
)

set(airplay_main_HEADER
	log.h
)

set(airplay_extra_SOURCES
	../ipc.c
)
			 
set(airplay_extra_HEADERS
	../ipc.h
)
 
add_executable(airplay-server
	${airplay_SRC}
	${airplay_HEADER}
	${airplay_main_SRC}
	${airplay_main_HEADER}
	${airplay_extra_SOURCES}
	${airplay_extra_HEADERS})

target_link_libraries(airplay-server
	libobs
	debug ${PROJECT_SOURCE_DIR}/third-party/lib/debug/plist.lib
	optimized ${PROJECT_SOURCE_DIR}/third-party/lib/relwithdebinfo/plist.lib
	debug ${PROJECT_SOURCE_DIR}/third-party/lib/debug/libcrypto.lib
	debug ${PROJECT_SOURCE_DIR}/third-party/lib/debug/libssl.lib
	optimized ${PROJECT_SOURCE_DIR}/third-party/lib/release/libcrypto.lib
	optimized ${PROJECT_SOURCE_DIR}/third-party/lib/release/libssl.lib
	Setupapi
	Crypt32
	Ws2_32
	w32-pthreads)

install_obs_core(airplay-server)
