# blueproximity.cpp

A C++ implementation of the BlueProximity application. This tool monitors the signal strength (RSSI) of a paired Bluetooth device (smartphone, watch, etc.) to automatically lock and unlock your screen when you move away or approach your computer.

## Features

- **Proximity Locking/Unlocking**: Automatically executes commands when a device moves out of or into range.
- **Bluetooth Support**: Supports both Classic Bluetooth and Bluetooth Low Energy (BLE) devices.
- **Configurable Thresholds**: Customize lock/unlock distances and durations to prevent false triggers.
- **Custom Commands**: Define your own shell commands for locking, unlocking, and proximity events.
- **Low Overhead**: Written in C++ for efficiency.

## Prerequisites

- Linux system with BlueZ stack.
- Bluetooth adapter.
- `libbluetooth-dev` package (for building).

## Building

To build the application, run:

```bash
make
```

This will produce the `BlueProximity` executable.

## Usage

Run the application with your device's MAC address:

```bash
./BlueProximity --mac <MAC_ADDRESS>
```

Or for a BLE device:

```bash
./BlueProximity --blemac <MAC_ADDRESS>
```

### Options

```
Usage: ./BlueProximity [options]
Options:
  -m, --mac, --btmac <address> Bluetooth MAC address (can be specified multiple times)
  --blemac <address>           Bluetooth Low Energy MAC address (can be specified multiple times)
  -c, --channel <channel>      RFCOMM channel (default: 1, ignored for BLE)
  --lock-distance <dist>       Distance to lock (default: 7)
  --unlock-distance <dist>     Distance to unlock (default: 4)
  --lock-duration <secs>       Duration to lock (default: 6)
  --unlock-duration <secs>     Duration to unlock (default: 1)
  --lock-cmd <command>         Command to lock screen
  --unlock-cmd <command>       Command to unlock screen
  --prox-cmd <command>         Command to run when in proximity
  --prox-interval <secs>       Interval for proximity command (default: 60)
  --buffer-size <size>         RSSI buffer size (default: 1)
  -d, --debug                  Enable debug output (AT commands)
  -h, --help                   Show this help message
```

## Configuration

Configuration is automatically saved to `~/.blueproximity/config` after the first run with valid arguments, or can be managed via command-line arguments.

## Known Issues

On my phone it showed an "unable to connect message" which got annoying fast. I just used my watch. It worked well enough for a holiday time off passion project. 
