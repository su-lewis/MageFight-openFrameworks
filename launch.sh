#!/bin/bash

# 1. Set up all the host libraries so Bazzite/SteamOS can find them
export LD_LIBRARY_PATH="/run/host/usr/lib64:/run/host/usr/lib:.:$LD_LIBRARY_PATH"
export LIBGL_DRIVERS_PATH="/run/host/usr/lib64/dri:/run/host/usr/lib/dri:$LIBGL_DRIVERS_PATH"

# 2. Strip Wayland variables so GLFW doesn't crash on Linux
unset WAYLAND_DISPLAY
unset XDG_SESSION_TYPE
export DISPLAY=:0

# 3. Force Dedicated Nvidia GPU
export __NV_PRIME_RENDER_OFFLOAD=1
export __GLX_VENDOR_LIBRARY_NAME="nvidia"

# 4. Run the game!
# If launched by Steam, Steam passes the game name automatically via $@
# If launched manually by you in the terminal, it defaults to ./MageFight
if [ $# -eq 0 ]; then
    exec ./MageFight
else
    exec "$@"
fi
