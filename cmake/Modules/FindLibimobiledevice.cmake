find_path(LIBIMOBILEDEVICE_INCLUDE_DIR NAMES libimobiledevice/libimobiledevice.h HINTS "${DepsPath}/include")

find_library(LIB_IMOBILEDEVICE NAMES libimobiledevice-1.0 PATHS "${DepsPath}/bin")

if(LIB_IMOBILEDEVICE)
	set(IMOBILEDEVICE_INCLUDE_DIRS ${LIBIMOBILEDEVICE_INCLUDE_DIR})
	set(IMOBILEDEVICE_LIBRARIES ${LIB_IMOBILEDEVICE})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libimobiledevice DEFAULT_MSG IMOBILEDEVICE_LIBRARIES IMOBILEDEVICE_INCLUDE_DIRS)
mark_as_advanced(IMOBILEDEVICE_INCLUDE_DIRS IMOBILEDEVICE_LIBRARIES)
