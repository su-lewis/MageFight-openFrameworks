#!/bin/bash

# 1. Log that we tried to start (Debug)
echo "StartSteam script triggered at $(date)" > /tmp/magefight_debug.log

# 2. Hardcode the IDs (Forces the game to identify as Spacewar)
export SteamAppId=480
export SteamGameId=480

# 3. Locate the Real Steam Path on your Host (Bazzite)
#    This helps the game find the Steam Client files.
if [ -d "$HOME/.local/share/Steam" ]; then
    export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
elif [ -d "$HOME/.steam/steam" ]; then
    export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.steam/steam"
fi

# 4. Launch the Container
#    We explicitly pass the variables into the bash command inside the container.
/usr/bin/distrobox-enter -n of_arch -- bash -c "export SteamAppId=480; export SteamGameId=480; export STEAM_COMPAT_CLIENT_INSTALL_PATH='$STEAM_COMPAT_CLIENT_INSTALL_PATH'; /home/lewis/of_workspace/openFrameworks/apps/myApps/MageFight/bin/run.sh"
