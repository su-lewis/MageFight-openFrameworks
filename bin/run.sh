#!/bin/bash

# Define the directory where this script is located (your 'bin' folder).
# This must be defined BEFORE setting the LOG_FILE for correct path resolution.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Define the log file path. It will be in the same directory as this script.
LOG_FILE="$DIR/steam_launch_log.txt"

# --- BEGIN SCRIPT EXECUTION LOGGING ---
# Redirect all subsequent stdout and stderr to the log file, overwriting it each time.
# We use 'exec' to ensure this applies to the whole script and the launched game.
exec > "$LOG_FILE" 2>&1

echo "--- run.sh script started at $(date) ---"
echo "Script directory: $DIR"
echo "Log file: $LOG_FILE"

# Set LD_LIBRARY_PATH for the container's environment.
# This ensures libGLEW.so.2.2 and other libraries are found.
export LD_LIBRARY_PATH="/usr/lib:$LD_LIBRARY_PATH"
echo "LD_LIBRARY_PATH set to: $LD_LIBRARY_PATH"

# Change into the script's directory. This is crucial for the game to find its 'data' folder.
cd "$DIR"
echo "Working directory changed to: $(pwd)"

# Execute the game executable.
# The output of MageFight will now also be captured by the 'exec' redirection.
echo "Attempting to execute: ./MageFight $*"
./MageFight "$@"

echo "--- run.sh script finished at $(date) ---"