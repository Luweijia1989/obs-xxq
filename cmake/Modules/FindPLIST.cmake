# Once done these will be defined:
#
#  PLIST_FOUND
#  PLIST_INCLUDE_DIRS
#  PLIST_LIBRARIES
#
# For use in OBS: 
#
#  PLIST_INCLUDE_DIR

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_PLIST QUIET plist)
endif()

find_path(PLIST_INCLUDE_DIR
	NAMES plist/plist.h
	HINTS
		ENV DepsPath
		${DepsPath}
		${_PLIST_INCLUDE_DIRS}

	PATH_SUFFIXES
		include)

find_library(PLIST_LIB
	NAMES plist-2.0
	HINTS
		ENV DepsPath
		${DepsPath}
		${_PLIST_LIBRARY_DIRS}

	PATH_SUFFIXES
		lib${_lib_suffix} lib
		libs${_lib_suffix} libs
		bin${_lib_suffix} bin)

find_library(PLIST_LIB++
	NAMES plist++-2.0
	HINTS
		ENV DepsPath
		${DepsPath}
		${_PLIST_LIBRARY_DIRS}

	PATH_SUFFIXES
		lib${_lib_suffix} lib
		libs${_lib_suffix} libs
		bin${_lib_suffix} bin)

if(PLIST_LIB AND PLIST_LIB++)
	set(PLIST_INCLUDE_DIRS ${PLIST_INCLUDE_DIR})
	set(PLIST_LIBRARIES ${PLIST_LIB} ${PLIST_LIB++})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PLIST DEFAULT_MSG PLIST_LIBRARIES PLIST_INCLUDE_DIRS)
mark_as_advanced(PLIST_INCLUDE_DIRS PLIST_LIBRARIES)


