cmake_minimum_required(VERSION 3.25)
project(xbox_iptv C)

add_executable(xbox-iptv
    src/main.c
    src/m3u.c
    src/http.c
)
set_target_properties(xbox-iptv PROPERTIES C_STANDARD 17 C_EXTENSIONS ON)
target_compile_options(xbox-iptv PRIVATE -Wall -Wextra -Werror)
xsdk_add_xex(xbox-iptv TITLE_ID 0x58544950 NAME "Xbox IPTV" VERSION 0.1.0.0)
