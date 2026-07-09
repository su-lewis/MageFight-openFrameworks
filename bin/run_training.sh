#!/bin/bash
export MAGEFIGHT_HEADLESS=1

# Kill old instances
pkill -9 -f MageFight
sleep 2

# Launch 4 instances, specifically setting the port env var
for i in {0..3}
do
   PORT=$((5555 + i))
   echo "Launching MageFight instance on port $PORT..."
   MAGEFIGHT_PORT=$PORT ./MageFight > /dev/null 2>&1 &
   sleep 1
done

echo "All 4 instances launched. Keep this terminal open."
wait
