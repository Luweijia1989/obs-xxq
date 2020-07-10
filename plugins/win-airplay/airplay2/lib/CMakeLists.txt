cmake_minimum_required(VERSION 3.4.1)
include_directories( curve25519 ed25519 playfair openssl/include dnssd plist
		fdk-aac/libAACdec/include
        fdk-aac/libAACenc/include
        fdk-aac/libFDK/include
        fdk-aac/libMpegTPDec/include
        fdk-aac/libMpegTPEnc/include
        fdk-aac/libPCMutils/include
        fdk-aac/libSBRdec/include
        fdk-aac/libSBRenc/include
        fdk-aac/libSYS/include
		fdk-aac/win32
        fdk-aac/libArithCoding/include
        fdk-aac/libDRCdec/include
        fdk-aac/libSACdec/include
        fdk-aac/libSACenc/include)

aux_source_directory(. play_src)
set(DIR_SRCS ${play_src})

file(GLOB_RECURSE fdkaac_SRC ${PROJECT_SOURCE_DIR}/fdk-aac *.cpp *.c)
file(GLOB_RECURSE fdkaac_HEADER ${PROJECT_SOURCE_DIR}/fdk-aac *.hpp *.h)

add_library( airplay
        STATIC
        ${DIR_SRCS}
		${fdkaac_HEADER}
		${fdkaac_SRC}
        )


target_link_libraries( airplay
		${CMAKE_CURRENT_SOURCE_DIR}/plist/libplist.a
		${CMAKE_CURRENT_SOURCE_DIR}/openssl/lib/libcryptoMTd.lib
		${CMAKE_CURRENT_SOURCE_DIR}/openssl/lib/libsslMTd.lib
		Crypt32
        Ws2_32	    
		legacy_stdio_definitions 
		curve25519
        ed25519
        playfair)
		
		
add_subdirectory(curve25519)
add_subdirectory(ed25519)
add_subdirectory(playfair)