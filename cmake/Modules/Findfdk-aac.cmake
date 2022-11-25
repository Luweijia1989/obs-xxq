find_path(FDKAAC_INCLUDE_DIR NAMES fdk-aac/aacenc_lib.h HINTS "${DepsPath}/include")

find_library(LIB_FDK_AAC NAMES fdk-aac PATHS "${DepsPath}/bin")

if(LIB_FDK_AAC)
	set(FDKAAC_INCLUDE_DIRS ${FDKAAC_INCLUDE_DIR})
	set(FDKAAC_LIBRARIES ${LIB_FDK_AAC})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenSSL DEFAULT_MSG FDKAAC_LIBRARIES FDKAAC_INCLUDE_DIRS)
mark_as_advanced(FDKAAC_INCLUDE_DIRS FDKAAC_LIBRARIES)
