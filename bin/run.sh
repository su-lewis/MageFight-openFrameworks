#!/bin/bash
# Define the directory where this script is located (your 'bin' folder)
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Define the log file path
LOG_FILE="$DIR/steam_launch_log.txt"

# Redirect all subsequent stdout and stderr to the log file
exec > "$LOG_FILE" 2>&1

echo "--- run.sh script started at $(date) ---"
echo "Script directory: $DIR"

# Change into the script's directory. This is crucial for the game to find 'data/'
cd "$DIR"
echo "Working directory changed to: $(pwd)"

# 1. Add current folder to library path (for libsteam_api.so)
# 2. Keep existing library paths (for container libraries)
export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
echo "LD_LIBRARY_PATH is: $LD_LIBRARY_PATH"

# Execute the game
echo "Attempting to execute: ./MageFight"
./MageFight "$@"

echo "--- run.sh script finished at $(date) ---"