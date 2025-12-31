#!/bin/bash
# Try to find display
export DISPLAY=:0
# Need XAUTHORITY for xset if running from service context often
export XAUTHORITY=/home/$(whoami)/.Xauthority

if [ -f "$XAUTHORITY" ]; then
    xset dpms force on
else
    echo "XAuthority not found, skipping proximity command"
fi
