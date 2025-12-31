#!/bin/bash
# Find session ID for user
ID=$(loginctl list-sessions --no-legend | grep $(whoami) | awk '{print $1}' | head -n 1)
if [ -n "$ID" ]; then
    echo "Locking session $ID"
    /usr/bin/loginctl lock-session "$ID"
else
    echo "No session found to lock"
fi
