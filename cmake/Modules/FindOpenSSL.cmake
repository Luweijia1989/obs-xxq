set(OPENSSL_ROOT_DIR ${DepsPath})

find_path(OPENSSL_INCLUDE_DIR NAMES openssl/ssl.h HINTS "${OPENSSL_ROOT_DIR}/include")

find_library(LIB_EAY NAMES libcrypto PATHS "${OPENSSL_ROOT_DIR}/bin")
find_library(SSL_EAY NAMES libssl PATHS "${OPENSSL_ROOT_DIR}/bin")
find_library(LIB_EAY_DEBUG NAMES libcrypto PATHS "${OPENSSL_ROOT_DIR}/bin/debug")
find_library(SSL_EAY_DEBUG NAMES libssl PATHS "${OPENSSL_ROOT_DIR}/bin/debug")

if(LIB_EAY AND SSL_EAY AND LIB_EAY_DEBUG AND SSL_EAY_DEBUG)
	set(OpenSSL_INCLUDE_DIR ${OPENSSL_INCLUDE_DIR})
	set(OpenSSL_LIBRARIES ${LIB_EAY} ${SSL_EAY} "crypt32;ws2_32")
	set(OpenSSL_DEBUG_LIBRARIES ${LIB_EAY_DEBUG} ${SSL_EAY_DEBUG} "crypt32;ws2_32")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenSSL DEFAULT_MSG OpenSSL_LIBRARIES OpenSSL_DEBUG_LIBRARIES OpenSSL_INCLUDE_DIR)
mark_as_advanced(OpenSSL_INCLUDE_DIR OpenSSL_LIBRARIES OpenSSL_DEBUG_LIBRARIES)
