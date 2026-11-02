cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0135 NEW) # Use extraction timestamp for FetchContent

project(MageFight VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Fetch Raylib v5.5
include(FetchContent)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
# Disable unnecessary raylib extras for fast build times
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# 2. Add magefight_raylib Executable
add_executable(magefight_raylib
    src/main.cpp
    src/GameApp.cpp
    src/core/DeterministicChecksum.cpp
    src/core/DeterministicHarness.cpp
    src/core/LockstepCommandQueue.cpp
    src/core/SnapshotFormat.cpp
)

# 3. Include Directories
target_include_directories(magefight_raylib PRIVATE
    src
    /home/lewis/of_workspace/openFrameworks/libs/json/include
)

# 4. Link Raylib (This automatically provides raylib.h)
target_link_libraries(magefight_raylib PRIVATE raylib)
