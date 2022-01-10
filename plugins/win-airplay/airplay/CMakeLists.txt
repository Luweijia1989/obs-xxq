project(airplay-server)

if(APPLE)
	set(_bit_suffix "")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_bit_suffix "64bit/")
else()
	set(_bit_suffix "32bit/")
endif()

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
	airplay-server.rc
)
			 
set(airplay_extra_HEADERS
	resource.h
)
 
add_executable(airplay-server
	${airplay_SRC}
	${airplay_HEADER}
	${airplay_main_SRC}
	${airplay_main_HEADER}
	${airplay_extra_SOURCES}
	${airplay_extra_HEADERS})
	
target_link_libraries(airplay-server
	debug ${PROJECT_SOURCE_DIR}/third-party/lib/debug/plist.lib
	optimized ${PROJECT_SOURCE_DIR}/third-party/lib/relwithdebinfo/plist.lib
	debug ${PROJECT_SOURCE_DIR}/third-party/lib/debug/fdk-aac.lib
	optimized ${PROJECT_SOURCE_DIR}/third-party/lib/relwithdebinfo/fdk-aac.lib
	debug ${PROJECT_SOURCE_DIR}/third-party/lib/debug/libcrypto.lib
	debug ${PROJECT_SOURCE_DIR}/third-party/lib/debug/libssl.lib
	optimized ${PROJECT_SOURCE_DIR}/third-party/lib/relwithdebinfo/libcrypto.lib
	optimized ${PROJECT_SOURCE_DIR}/third-party/lib/relwithdebinfo/libssl.lib
	Setupapi
	Crypt32
	Ws2_32
	Dbghelp
	Shlwapi
	w32-pthreads
	mirror-ipc)

install_obs_core(airplay-server)

function(install_dependency target)
add_custom_command(TARGET airplay-server POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/third-party/bin/$<CONFIGURATION>/${target}.pdb"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}${target}.pdb"
		VERBATIM)
add_custom_command(TARGET airplay-server POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/third-party/bin/$<CONFIGURATION>/${target}.dll"
			"${OBS_OUTPUT_DIR}/$<CONFIGURATION>/bin/${_bit_suffix}${target}.dll"
		VERBATIM)
add_custom_command(TARGET airplay-server POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/third-party/bin/$<CONFIGURATION>/${target}.pdb"
			"$<TARGET_FILE_DIR:airplay-server>/${target}.pdb"
		VERBATIM)
add_custom_command(TARGET airplay-server POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy
			"${PROJECT_SOURCE_DIR}/third-party/bin/$<CONFIGURATION>/${target}.dll"
			"$<TARGET_FILE_DIR:airplay-server>/${target}.dll"
		VERBATIM)
endfunction()
 
install_dependency("plist") 
