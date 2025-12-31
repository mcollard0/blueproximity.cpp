#!/bin/bash
# Find session ID for user
ID=$(loginctl list-sessions --no-legend | grep $(whoami) | awk '{print $1}' | head -n 1)
if [ -n "$ID" ]; then
    echo "Unlocking session $ID"
    /usr/bin/loginctl unlock-session "$ID"
else
    echo "No session found to unlock"
fi
