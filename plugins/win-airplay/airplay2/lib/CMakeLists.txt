project(airplay2)

include_directories(crypto curve25519 ed25519 playfair plist
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
		${PROJECT_SOURCE_DIR}/../include
        fdk-aac/libArithCoding/include
        fdk-aac/libDRCdec/include
        fdk-aac/libSACdec/include
        fdk-aac/libSACenc/include
        )


aux_source_directory(. play_src)
set(DIR_SRCS ${play_src})

file(GLOB_RECURSE fdkaac_SRC ${PROJECT_SOURCE_DIR}/fdk-aac *.cpp *.c)
file(GLOB_RECURSE fdkaac_HEADER ${PROJECT_SOURCE_DIR}/fdk-aac *.hpp *.h)

add_library( airplay2
        STATIC
        ${DIR_SRCS}
		${FDK_SRCS}
		${fdkaac_HEADER}
		${fdkaac_SRC}
        )

target_link_libraries( airplay2
		${PROJECT_SOURCE_DIR}/plist/libplist.a
		legacy_stdio_definitions
        crypto
        curve25519
        ed25519
        playfair)

add_subdirectory(crypto)
add_subdirectory(curve25519)
add_subdirectory(ed25519)
add_subdirectory(playfair)