set(OPENSSL_ROOT_DIR "/usr/local/openssl")
set(OPENSSL_CRYPTO_LIBRARY "/usr/local/openssl/lib64/libcrypto.so.3")
set(OPENSSL_SSL_LIBRARY "/usr/local/openssl/lib64/libssl.so.3")
set(OPENSSL_INCLUDE_DIR "/usr/local/openssl/include")

find_package(OpenSSL REQUIRED)

message(STATUS "OpenSSL Found: ${OPENSSL_VERSION}")

include_directories(${OPENSSL_INCLUDE_DIR})
link_directories("/usr/local/openssl/lib64")
