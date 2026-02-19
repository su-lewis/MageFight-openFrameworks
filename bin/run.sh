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
export LD_LIBRARY_PATH=.:./libs/steam/lib:$LD_LIBRARY_PATH

# 8. Ensure STEAM compat path is set
if [ -d "$HOME/.local/share/Steam" ]; then
  export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
elif [ -d "$HOME/.steam/steam" ]; then
  export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.steam/steam"
fi

echo "Starting executable..."
./MageFight "$@"
