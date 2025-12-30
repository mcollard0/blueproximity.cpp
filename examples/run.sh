#!/bin/bash
# BlueProximity Start Script Example
# Device 1: Example Phone
# MAC: FF:FF:FF:FF:FF:FF
# Channel: 19

# Device 2: Example Watch
# MAC: FF:FF:FF:FF:FF:FF
# Channel: 4

./BlueProximity \
    --lock-distance 30 \
    --unlock-distance 20 \
    --unlock-duration 2 \
    --channel 4  --btmac FF:FF:FF:FF:FF:FF \
    "$@"
