find_path(FDKAAC_INCLUDE_DIR NAMES fdk-aac/aacenc_lib.h HINTS "${DepsPath}/include")

find_library(LIB_FDK_AAC NAMES fdk-aac PATHS "${DepsPath}/bin")
find_library(LIB_FDK_AAC_DEBUG NAMES fdk-aac PATHS "${DepsPath}/bin/debug")

if(LIB_FDK_AAC AND LIB_FDK_AAC_DEBUG)
	set(FDKAAC_INCLUDE_DIRS ${FDKAAC_INCLUDE_DIR})
	set(FDKAAC_LIBRARIES ${LIB_FDK_AAC})
	set(FDKAAC_DEBUG_LIBRARIES ${LIB_FDK_AAC_DEBUG})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(fdk-aac DEFAULT_MSG FDKAAC_LIBRARIES FDKAAC_DEBUG_LIBRARIES FDKAAC_INCLUDE_DIRS)
mark_as_advanced(FDKAAC_INCLUDE_DIRS FDKAAC_LIBRARIES FDKAAC_DEBUG_LIBRARIES)
