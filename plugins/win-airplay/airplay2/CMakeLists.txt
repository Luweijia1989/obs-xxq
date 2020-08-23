project(airplay-server)

include_directories( lib/ lib/curve25519 lib/ed25519 lib/playfair lib/openssl/include lib/dnssd lib/plist
	lib/fdk-aac/libAACdec/include
        lib/fdk-aac/libAACenc/include
        lib/fdk-aac/libFDK/include
        lib/fdk-aac/libMpegTPDec/include
        lib/fdk-aac/libMpegTPEnc/include
        lib/fdk-aac/libPCMutils/include
        lib/fdk-aac/libSBRdec/include
        lib/fdk-aac/libSBRenc/include
        lib/fdk-aac/libSYS/include
	lib/fdk-aac/win32
        lib/fdk-aac/libArithCoding/include
        lib/fdk-aac/libDRCdec/include
        lib/fdk-aac/libSACdec/include
        lib/fdk-aac/libSACenc/include)

aux_source_directory(. play_src)
set(DIR_SRCS ${play_src})

file(GLOB_RECURSE fdkaac_SRC ${PROJECT_SOURCE_DIR}/lib/fdk-aac *.cpp *.c)
file(GLOB_RECURSE fdkaac_HEADER ${PROJECT_SOURCE_DIR}/lib/fdk-aac *.hpp *.h)

set(iosairplay_extra_SOURCES
	../ipc.c
)
			 
set(iosairplay_extra_HEADERS
	../ipc.h
)

add_executable( airplay-server
	main.cpp
	${DIR_SRCS}
	${fdkaac_HEADER}
	${fdkaac_SRC}
	${iosairplay_extra_HEADERS}
	${iosairplay_extra_SOURCES})

target_link_libraries( airplay-server
	${PROJECT_SOURCE_DIR}/lib/plist/libplist.a
	debug ${PROJECT_SOURCE_DIR}/lib/openssl/lib/libcryptoMTd.lib
	debug ${PROJECT_SOURCE_DIR}/lib/openssl/lib/libsslMTd.lib
	optimized ${PROJECT_SOURCE_DIR}/lib/openssl/lib/libcryptoMT.lib
	optimized ${PROJECT_SOURCE_DIR}/lib/openssl/lib/libsslMT.lib
	ipc-util
	Crypt32
	Ws2_32	    
	legacy_stdio_definitions 
	curve25519
	ed25519
	playfair)
		
		
add_subdirectory(lib/curve25519)
add_subdirectory(lib/ed25519)
add_subdirectory(lib/playfair)

install_obs_core(airplay-server)
