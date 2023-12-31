find_path(LIBUSBMUXD_INCLUDE_DIR NAMES usbmuxd.h HINTS "${DepsPath}/include")

find_library(LIB_USBMUXD NAMES libusbmuxd PATHS "${DepsPath}/bin")
find_library(LIB_USBMUXD_DEBUG NAMES libusbmuxd PATHS "${DepsPath}/bin/debug")

if(LIB_USBMUXD AND LIB_USBMUXD_DEBUG)
	set(LIBUSBMUXD_INCLUDE_DIRS ${LIBUSBMUXD_INCLUDE_DIR})
	set(LIBUSBMUXD_LIBRARIES ${LIB_USBMUXD})
	set(LIBUSBMUXD_DEBUG_LIBRARIES ${LIB_USBMUXD_DEBUG})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libusbmuxd DEFAULT_MSG LIBUSBMUXD_LIBRARIES LIBUSBMUXD_DEBUG_LIBRARIES LIBUSBMUXD_INCLUDE_DIRS)
mark_as_advanced(LIBUSBMUXD_INCLUDE_DIRS LIBUSBMUXD_DEBUG_LIBRARIES LIBUSBMUXD_LIBRARIES)
