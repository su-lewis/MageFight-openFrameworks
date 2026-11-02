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

# 2. Add MageFightClient Executable
add_executable(MageFightClient
    src/main.cpp
    src/core/Simulation.cpp
)

# 3. Include Directories
target_include_directories(MageFightClient PRIVATE
    src
)

# 4. Link Raylib (This automatically provides raylib.h to src/main.cpp)
target_link_libraries(MageFightClient PRIVATE raylib)