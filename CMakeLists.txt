cmake_minimum_required(VERSION 3.25)
project(xbox_iptv C)

if(NOT DEFINED XSDK)
    message(FATAL_ERROR "Configure this project with the XSDK toolchain")
endif()
if(NOT DEFINED MBEDTLS_XENON_DIR OR NOT DEFINED CURL_XENON_DIR)
    message(FATAL_ERROR "MBEDTLS_XENON_DIR and CURL_XENON_DIR are required")
endif()

set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${MBEDTLS_XENON_DIR}" mbedtls)

set(MBEDTLS_INCLUDE_DIR "${MBEDTLS_XENON_DIR}/include" CACHE PATH "" FORCE)
set(MBEDTLS_LIBRARY mbedtls CACHE STRING "" FORCE)
set(MBEDX509_LIBRARY mbedx509 CACHE STRING "" FORCE)
set(MBEDCRYPTO_LIBRARY mbedcrypto CACHE STRING "" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory("${CURL_XENON_DIR}" curl)

add_executable(xbox-iptv
    src/main.c
    src/m3u.c
    src/http.c
    src/hls.c
    src/epg.c
    src/net_playlist.c
)
set_target_properties(xbox-iptv PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
target_compile_options(xbox-iptv PRIVATE -Wall -Wextra -Werror)
target_link_libraries(xbox-iptv PRIVATE CURL::libcurl mbedtls)
xsdk_add_xex(xbox-iptv TITLE_ID 0x58544950 NAME "Xbox IPTV" VERSION 0.2.0.0)

configure_file("${MBEDTLS_XENON_DIR}/programs/xsdk/cacert.pem"
               "${CMAKE_CURRENT_BINARY_DIR}/cacert.pem" COPYONLY)
