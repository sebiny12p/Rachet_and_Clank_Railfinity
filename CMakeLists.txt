cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
project(RatchetAndClankRailfinity LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (ANDROID)
    set(PLATFORM "Android" CACHE STRING "" FORCE)
    set(GRAPHICS "GRAPHICS_API_OPENGL_ES2" CACHE STRING "" FORCE)
endif()

# Reuse cached raylib source if available on this machine for fast local builds,
# otherwise automatically download Raylib 5.0 via FetchContent.
set(LOCAL_RAYLIB_CACHE "C:/Users/Sebas/source/repos/InsaSwarm/simulator/build/_deps/raylib-src")
if (EXISTS "${LOCAL_RAYLIB_CACHE}/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_RAYLIB "${LOCAL_RAYLIB_CACHE}" CACHE PATH "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
    GIT_SHALLOW TRUE
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

set(SOURCES
    src/main.cpp
    src/RailGame.cpp
    src/RailRenderer3D.cpp
    src/RailAudio.cpp
)

if (ANDROID)
    add_library(main SHARED ${SOURCES})
    target_include_directories(main PRIVATE include "${ANDROID_NDK}/sources/android/native_app_glue")
    target_link_options(main PRIVATE "-u" "ANativeActivity_onCreate")
    target_link_libraries(main PRIVATE raylib android log EGL GLESv2 OpenSLES atomic m)
else()
    add_executable(RatchetAndClankRailfinity ${SOURCES})
    target_include_directories(RatchetAndClankRailfinity PRIVATE include)
    target_link_libraries(RatchetAndClankRailfinity PRIVATE raylib)
    add_custom_target(copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            ${CMAKE_CURRENT_BINARY_DIR}/assets
    )
    add_dependencies(RatchetAndClankRailfinity copy_assets)
endif()

if (WIN32)
    target_link_libraries(RatchetAndClankRailfinity PRIVATE winmm gdi32 opengl32)
    if (MINGW)
        target_link_options(RatchetAndClankRailfinity PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()
