set(OPENSSL_ROOT_DIR ${DepsPath})

find_path(OPENSSL_INCLUDE_DIR NAMES openssl/ssl.h HINTS "${OPENSSL_ROOT_DIR}/include")

find_library(LIB_EAY NAMES libcrypto PATHS "${OPENSSL_ROOT_DIR}/bin")
find_library(SSL_EAY NAMES libssl PATHS "${OPENSSL_ROOT_DIR}/bin")

if(LIB_EAY AND SSL_EAY)
	set(OpenSSL_INCLUDE_DIR ${OPENSSL_INCLUDE_DIR})
	set(OpenSSL_LIBRARIES ${LIB_EAY} ${SSL_EAY} "crypt32;ws2_32")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenSSL DEFAULT_MSG OpenSSL_LIBRARIES OpenSSL_INCLUDE_DIR)
mark_as_advanced(OpenSSL_INCLUDE_DIR OpenSSL_LIBRARIES)
