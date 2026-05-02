#!/bin/bash

# 1. Get the directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# 2. Define the Log File
LOG_FILE="$DIR/steam_launch_log.txt"

# 3. Truncate the log file on startup so previous runs don't accumulate,
#    then redirect EVERYTHING to the log file.
: > "$LOG_FILE"
exec >> "$LOG_FILE" 2>&1

echo "--- Launching MageFight at $(date) ---"
echo "Script directory: $DIR"

# 4. Force System to working directory (Fixes Textures)
cd "$DIR"

# 5. Sanitize LD_PRELOAD (drop missing libs and 32-bit overlay)
echo "Original LD_PRELOAD=$LD_PRELOAD"
orig_LD_PRELOAD="${LD_PRELOAD:-}"
new_LD_PRELOAD=""
IFS=':'
for p in $orig_LD_PRELOAD; do
  if [ -z "$p" ]; then
    continue
  fi
  if [ ! -e "$p" ]; then
    echo "Dropping missing LD_PRELOAD entry: $p"
    continue
  fi
  case "$p" in
    *gameoverlayrenderer.so*ubuntu12_32*)
      echo "Dropping 32-bit overlay: $p"
      continue
      ;;
    */extest/libextest.so)
      echo "Dropping extest entry: $p"
      continue
      ;;
  esac
  if [ -z "$new_LD_PRELOAD" ]; then
    new_LD_PRELOAD="$p"
  else
    new_LD_PRELOAD="$new_LD_PRELOAD:$p"
  fi
done
unset IFS

if [ -f "$HOME/.local/share/Steam/ubuntu12_64/gameoverlayrenderer.so" ]; then
  echo "Adding 64-bit overlay to LD_PRELOAD"
  if [ -z "$new_LD_PRELOAD" ]; then
    new_LD_PRELOAD="$HOME/.local/share/Steam/ubuntu12_64/gameoverlayrenderer.so"
  else
    new_LD_PRELOAD="$HOME/.local/share/Steam/ubuntu12_64/gameoverlayrenderer.so:$new_LD_PRELOAD"
  fi
fi

if [ -n "$new_LD_PRELOAD" ]; then
  export LD_PRELOAD="$new_LD_PRELOAD"
else
  unset LD_PRELOAD
fi

echo "Final LD_PRELOAD=$LD_PRELOAD"

# 6. Force Steam App ID (Fixes Initialization)
echo "480" > steam_appid.txt

# 7. Set Library Paths (Fixes Missing DLLs/Libs)
export HOST_NVIDIA_LIB_PATH=/run/host/usr/lib64:/run/host/usr/lib
export HOST_GL_DRIVERS_PATH=/run/host/usr/lib64/dri:/run/host/usr/lib/dri
export LD_LIBRARY_PATH="$HOST_NVIDIA_LIB_PATH:.:./libs/steam/lib:$LD_LIBRARY_PATH"
export LIBGL_DRIVERS_PATH="$HOST_GL_DRIVERS_PATH:$LIBGL_DRIVERS_PATH"

# 8. Force X11 so GLFW uses GLX instead of Wayland.
#    This is required because the application is using GLX and the window
#    platform must be X11/XWayland, not native Wayland.
unset WAYLAND_DISPLAY
unset XDG_SESSION_TYPE
export XDG_SESSION_TYPE=x11
export DISPLAY=${DISPLAY:-:0}

# 9. Prefer NVIDIA PRIME render offload on hybrid systems
export __NV_PRIME_RENDER_OFFLOAD=1
export __GLX_VENDOR_LIBRARY_NAME=nvidia
export __VK_LAYER_NV_optimus=NVIDIA_only

echo "Host NVIDIA lib path: $HOST_NVIDIA_LIB_PATH"
echo "Host GL drivers path: $HOST_GL_DRIVERS_PATH"
echo "NVIDIA offload env: __NV_PRIME_RENDER_OFFLOAD=${__NV_PRIME_RENDER_OFFLOAD}, __GLX_VENDOR_LIBRARY_NAME=${__GLX_VENDOR_LIBRARY_NAME}"

# 9. Ensure STEAM compat path is set
if [ -d "$HOME/.local/share/Steam" ]; then
  export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
elif [ -d "$HOME/.steam/steam" ]; then
  export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.steam/steam"
fi

echo "Starting executable..."
./MageFight "$@"
