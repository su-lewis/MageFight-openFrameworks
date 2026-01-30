#!/bin/bash

# Define a log file location INSIDE the project's bin directory
LOG_FILE="$DIR/internal_run_log.txt"
exec > >(tee -a "$LOG_FILE") 2>&1
echo "--- run.sh started at $(date) ---"

# Get the directory where this script is located (i.e., the 'bin' folder).
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
echo "Current script directory: $DIR"

# Prepend the container's main library path to the search path.
export LD_LIBRARY_PATH="/usr/lib:$LD_LIBRARY_PATH"
echo "LD_LIBRARY_PATH set to: $LD_LIBRARY_PATH"

# Change into the script's directory so the game can find its 'data' folder.
cd "$DIR"
echo "Changed directory to: $(pwd)"

# Execute the game executable, passing along any arguments Steam provides.
echo "Attempting to execute: ./MageFight $*"
./MageFight "$@"

echo "--- run.sh finished at $(date) ---"