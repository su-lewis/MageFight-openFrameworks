#!/bin/bash

# 1. Strip Wayland variables so GLFW doesn't crash on Bazzite
unset WAYLAND_DISPLAY
unset XDG_SESSION_TYPE
export DISPLAY=:0

# 2. Move into the bin directory using absolute path
cd "/home/lewis/of_workspace/openFrameworks/apps/myApps/MageFight/bin"

# 3. Run the game
./MageFight
