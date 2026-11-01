cmake_minimum_required(VERSION 3.20)
project(MageFightRaylib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Keep raylib vendored by CMake so this port can build without system packages.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RSHAPES ON CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RTEXTURES ON CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RTEXT ON CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RMODELS ON CACHE BOOL "" FORCE)
set(SUPPORT_MODULE_RAUDIO ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)
FetchContent_MakeAvailable(raylib)

add_executable(magefight_raylib
    src/main.cpp
    src/GameApp.cpp
    src/core/DeterministicChecksum.cpp
    src/core/DeterministicHarness.cpp
    src/core/LockstepCommandQueue.cpp
    src/core/SnapshotFormat.cpp
)

target_include_directories(magefight_raylib PRIVATE src)
target_link_libraries(magefight_raylib PRIVATE raylib)

if(UNIX AND NOT APPLE)
    target_link_libraries(magefight_raylib PRIVATE m dl pthread rt X11)
endif()
