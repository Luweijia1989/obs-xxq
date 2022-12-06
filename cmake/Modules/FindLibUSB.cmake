find_path(LIBUSB_INCLUDE_DIR NAMES lusb0_usb.h HINTS "${DepsPath}/include")

find_library(LIB_USB NAMES libusb-1.0 PATHS "${DepsPath}/bin")
find_library(LIB_USB_WIN32 NAMES libusb0 PATHS "${DepsPath}/bin")
find_library(LIB_USB_DEBUG NAMES libusb-1.0 PATHS "${DepsPath}/bin/debug")
find_library(LIB_USB_WIN32_DEBUG NAMES libusb0 PATHS "${DepsPath}/bin/debug")

if(LIB_USB AND LIB_USB_WIN32 AND LIB_USB_DEBUG AND LIB_USB_WIN32_DEBUG)
	set(LIB_USB_INCLUDE_DIRS ${LIBUSB_INCLUDE_DIR})
	set(LIB_USB_LIBRARIES ${LIB_USB} ${LIB_USB_WIN32})
	set(LIB_USB_DEBUG_LIBRARIES ${LIB_USB_DEBUG} ${LIB_USB_WIN32_DEBUG})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibUSB DEFAULT_MSG LIB_USB_LIBRARIES LIB_USB_DEBUG_LIBRARIES LIB_USB_INCLUDE_DIRS)
mark_as_advanced(LIB_USB_INCLUDE_DIRS LIB_USB_LIBRARIES LIB_USB_DEBUG_LIBRARIES)
