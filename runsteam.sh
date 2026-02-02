#!/bin/bash

# Runscript to be used as Steam target (Add as Non‑Steam game entry)
# - sanitizes LD_PRELOAD (drops 32-bit overlay / missing libs)
# - prefers 64-bit Steam overlay when available
# - ensures SteamAppId/compat path are exported
# - starts Steam if not running
# - calls the real bin/run.sh

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
LOG="$DIR/steam_launch_log.txt"

# Append logs for debugging
exec >> "$LOG" 2>&1

echo "--- Launching RunSteam wrapper at $(date) ---"

echo "Original LD_PRELOAD=$LD_PRELOAD"

# Sanitize LD_PRELOAD: remove non-existent files and 32-bit overlay
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

# Prefer the 64-bit overlay if present
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

# Ensure Steam compat path is set for runtime libraries
if [ -d "$HOME/.local/share/Steam" ]; then
  export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
elif [ -d "$HOME/.steam/steam" ]; then
  export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.steam/steam"
fi

echo "STEAM_COMPAT_CLIENT_INSTALL_PATH=$STEAM_COMPAT_CLIENT_INSTALL_PATH"

# Force Steam App ID so Steam API initializes to the correct app
export SteamAppId=480
export SteamGameId=480

# If Steam isn't running, try to launch it (best-effort)
if ! pgrep -x steam >/dev/null 2>&1; then
  echo "Steam not running — launching steam in background"
  nohup steam >/dev/null 2>&1 &
  # Give Steam a moment to initialize
  sleep 2
fi

# Finally execute the game startup script
cd "$DIR" || exit 1
./bin/run.sh "$@"
