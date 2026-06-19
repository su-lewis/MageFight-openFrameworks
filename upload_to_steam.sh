#!/bin/bash
# Run this script to instantly push the 'bin' folder to Steam!

APP_ID="4329880"
DEPOT_ID="4329881"
STEAM_USER="planetlewis"
STEAM_PASS="TheHouseAlwaysWinsPart1@6"
GAME_DIR="/var/home/lewis/of_workspace/openFrameworks/apps/myApps/MageFight/bin"

# 1. Dynamically generate the VDF files in the /tmp/ directory so they don't clutter your workspace
cat <<EOF > /tmp/app_build_$APP_ID.vdf
"appbuild"
{
	"appid" "$APP_ID"
	"desc" "Auto-upload from Linux"
	"buildoutput" "/tmp/"
	"contentroot" ""
	"setlive" "default" // Automatically sets this build live to the default branch!
	"depots"
	{
		"$DEPOT_ID" "/tmp/depot_build_$DEPOT_ID.vdf"
	}
}
EOF

cat <<EOF > /tmp/depot_build_$DEPOT_ID.vdf
"DepotBuildConfig"
{
	"DepotID" "$DEPOT_ID"
	"contentroot" "$GAME_DIR"
	"FileMapping"
	{
		"LocalPath" "*"
		"DepotPath" "."
		"recursive" "1"
	}
}
EOF

# 2. Run SteamCMD using the temporary scripts
~/steamcmd/steamcmd.sh +login $STEAM_USER $STEAM_PASS +run_app_build /tmp/app_build_$APP_ID.vdf +quit

echo "Upload Complete!"
