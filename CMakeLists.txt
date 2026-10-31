cmake_minimum_required(VERSION 3.21)

project(TerminalMusicPlayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

# FTXUI ships a proper CMake package.
find_package(ftxui CONFIG REQUIRED)

# The vcpkg miniaudio port is header-only and does not ship a CMake config,
# so locate the header directly.
find_path(MINIAUDIO_INCLUDE_DIR NAMES miniaudio.h REQUIRED)
message(STATUS "miniaudio header: ${MINIAUDIO_INCLUDE_DIR}")

add_executable(music-player
    src/main.cpp
    src/audio/AudioEngine.cpp
    src/core/Playlist.cpp
    src/ui/PlayerApp.cpp
)

target_include_directories(music-player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MINIAUDIO_INCLUDE_DIR}
)

target_link_libraries(music-player PRIVATE
    ftxui::component
    ftxui::dom
    ftxui::screen
)

if(WIN32)
    # WASAPI / DirectSound / WinMM / MMCSS backends used by miniaudio.
    target_link_libraries(music-player PRIVATE ole32 winmm dsound avrt)
endif()

set_target_properties(music-player PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
