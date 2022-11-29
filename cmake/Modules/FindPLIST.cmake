find_path(PLIST_INCLUDE_DIR NAMES plist.h HINTS "${DepsPath}/include")

find_library(PLIST_LIB NAMES plist-2.0 PATHS "${DepsPath}/bin")
find_library(PLIST_LIB++ NAMES plist++-2.0 PATHS "${DepsPath}/bin")
find_library(PLIST_LIB_DEBUG NAMES plist-2.0 PATHS "${DepsPath}/bin/debug")
find_library(PLIST_LIB++_DEBUG NAMES plist++-2.0 PATHS "${DepsPath}/bin/debug")

if(PLIST_LIB AND PLIST_LIB++ AND PLIST_LIB_DEBUG AND PLIST_LIB++_DEBUG)
	set(PLIST_INCLUDE_DIRS ${PLIST_INCLUDE_DIR})
	set(PLIST_LIBRARIES ${PLIST_LIB} ${PLIST_LIB++})
	set(PLIST_DEBUG_LIBRARIES ${PLIST_LIB_DEBUG} ${PLIST_LIB++_DEBUG})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PLIST DEFAULT_MSG PLIST_LIBRARIES PLIST_DEBUG_LIBRARIES PLIST_INCLUDE_DIRS)
mark_as_advanced(PLIST_INCLUDE_DIRS PLIST_LIBRARIES PLIST_DEBUG_LIBRARIES)


