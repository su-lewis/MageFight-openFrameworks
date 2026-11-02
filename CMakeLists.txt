cmake_minimum_required(VERSION 3.14)
project(MageFight)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically fetch and build Raylib locally (Zero system installation required!)
include(FetchContent)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
)
FetchContent_MakeAvailable(raylib)

# Add our executable combining core logic and the Raylib main loop
add_executable(MageFightClient 
    src/main.cpp
)

# Link Raylib and include core directories
target_include_directories(MageFightClient PRIVATE src src/core)
target_link_libraries(MageFightClient PRIVATE raylib)

# On Linux, Raylib requires these system graphics and windowing libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(MageFightClient PRIVATE GL m pthread dl rt X11)
endif()
