project(airplay-server)

find_package(PLIST REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(fdk-aac REQUIRED)

if(APPLE)
	set(_bit_suffix "")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "64bit/")
	set(_lib_bit_suffix "x64")
else()
	set(_bit_suffix "32bit/")
	set(_lib_bit_suffix "x86")
endif()

include_directories(${PROJECT_SOURCE_DIR}/third-party/include)
include_directories(${PROJECT_SOURCE_DIR}/lib)

add_definitions(-DWIN32_LEAN_AND_MEAN -DHAS_DUMMY_RENDERER)

file(GLOB_RECURSE airplay_SRC ${PROJECT_SOURCE_DIR}/lib ${PROJECT_SOURCE_DIR}/llhttp ${PROJECT_SOURCE_DIR}/playfair *.c)
file(GLOB_RECURSE airplay_HEADER ${PROJECT_SOURCE_DIR}/lib ${PROJECT_SOURCE_DIR}/llhttp ${PROJECT_SOURCE_DIR}/playfair *.h)

set(airplay_main_SRC
	rpiplay.cpp
)

set(airplay_extra_SOURCES
	airplay-server.rc
)
			 
set(airplay_extra_HEADERS
	resource.h
)
 
add_executable(airplay-server
	${airplay_SRC}
	${airplay_HEADER}
	${airplay_main_SRC}
	${airplay_extra_SOURCES}
	${airplay_extra_HEADERS})
	
target_link_libraries(airplay-server
	${PLIST_LIBRARIES}
	${OpenSSL_LIBRARIES}
	${FDKAAC_LIBRARIES}
	Setupapi
	Dbghelp
	Shlwapi
	w32-pthreads
	mirror-ipc)
set_target_properties(airplay-server PROPERTIES FOLDER "plugins/win-airplay")

install_obs_core(airplay-server)
