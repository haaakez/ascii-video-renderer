cmake_minimum_required(VERSION 3.18)

project(ascii-video
    VERSION 0.1.0
    DESCRIPTION "CPU ASCII video renderer"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.8)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.18)
pkg_check_modules(GSTREAMER_APP REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.18)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.18)
pkg_check_modules(EPOXY REQUIRED IMPORTED_TARGET epoxy)

add_executable(ascii-video
    src/main.c
)

target_link_libraries(ascii-video PRIVATE
    PkgConfig::GTK4
    PkgConfig::CAIRO
    PkgConfig::GSTREAMER
    PkgConfig::GSTREAMER_APP
    PkgConfig::GSTREAMER_VIDEO
    PkgConfig::EPOXY
    m
)

target_compile_options(ascii-video PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if(DEFINED ASCII_VIDEO_FFMPEG_PATH)
    target_compile_definitions(ascii-video PRIVATE
        ASCII_VIDEO_FFMPEG_PATH="${ASCII_VIDEO_FFMPEG_PATH}"
    )
endif()

include(GNUInstallDirs)

install(TARGETS ascii-video
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(FILES README.md
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)
