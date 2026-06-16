#!/bin/bash

# --- 1. SETUP & LOGGING ---
LOG="/tmp/magefight_steam_launch.log"
exec > "$LOG" 2>&1
echo "--- Launching MageFight via Steam on Bazzite/Distrobox at $(date) ---"

# --- 2. SANITIZE LD_PRELOAD (Fixes Linux Steam Overlay Crashes) ---
orig_LD_PRELOAD="${LD_PRELOAD:-}"
new_LD_PRELOAD=""
IFS=':'
for p in $orig_LD_PRELOAD; do
  if [ -z "$p" ]; then continue; fi
  if [ ! -e "$p" ]; then continue; fi
  case "$p" in
    *gameoverlayrenderer.so*ubuntu12_32*)
      echo "Dropping 32-bit overlay: $p"
      continue
      ;;
    */extest/libextest.so)
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
  if [ -z "$new_LD_PRELOAD" ]; then
    new_LD_PRELOAD="$HOME/.local/share/Steam/ubuntu12_64/gameoverlayrenderer.so"
  else
    new_LD_PRELOAD="$HOME/.local/share/Steam/ubuntu12_64/gameoverlayrenderer.so:$new_LD_PRELOAD"
  fi
fi

export LD_PRELOAD="$new_LD_PRELOAD"
echo "Final LD_PRELOAD=$LD_PRELOAD"

# --- 3. STEAM APP IDS & PATHS ---
if [ -d "$HOME/.local/share/Steam" ]; then
    export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
elif [ -d "$HOME/.steam/steam" ]; then
    export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.steam/steam"
fi

# --- 4. LAUNCH THE DISTROBOX CONTAINER ---
echo "Entering distrobox to execute run.sh..."

/usr/bin/distrobox-enter -n of_arch -- bash -c "export SteamAppId=480; export SteamGameId=480; export STEAM_COMPAT_CLIENT_INSTALL_PATH='$STEAM_COMPAT_CLIENT_INSTALL_PATH'; export LD_PRELOAD='$LD_PRELOAD'; /home/lewis/of_workspace/openFrameworks/apps/myApps/MageFight/bin/run.sh"

echo "Launch sequence finished."
