#!/bin/bash
# BlueProximity Start Script

# Device 1: S23 Ultra (Phone)
# MAC: 6C:AC:C2:D5:24:AB
# Channel: 19 (Determined from old config)

# Device 2: Galaxy Watch7 (Watch)
# MAC: 24:24:B7:9F:1B:47
# Channel: 4 (Handsfree Service - Stable)

./BlueProximity \
    --lock-distance 100 \
    --unlock-distance 85 \
    --unlock-duration 2 \
    --channel 19 --btmac 6C:AC:C2:D5:24:AB \
    --channel 4  --btmac 24:24:B7:9F:1B:47 \
    "$@"
