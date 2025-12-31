# blueproximity.cpp

A C++ implementation of the BlueProximity application. This tool monitors the signal strength (RSSI) of a paired Bluetooth device (smartphone, watch, etc.) to automatically lock and unlock your screen when you move away or approach your computer.

## Features

- **Proximity Locking/Unlocking**: Automatically executes commands when a device moves out of or into range.
- **Lock State Synchronization**: Periodically syncs with system lock state to handle timeout-based locks (checks every 30 seconds).
- **Bluetooth Support**: Supports both Classic Bluetooth and Bluetooth Low Energy (BLE) devices.
- **Configurable Thresholds**: Customize lock/unlock distances and durations to prevent false triggers.
- **Custom Commands**: Define your own shell commands for locking, unlocking, and proximity events.
- **Low Overhead**: Written in C++ for efficiency.

## Prerequisites

- Linux system with BlueZ stack.
- Bluetooth adapter.
- `libbluetooth-dev` package (for building).
- `systemd-logind` (for lock state synchronization).

## Compatibility

**Desktop Environments:**
- ✅ **GNOME/GDM** (Ubuntu, Fedora, RHEL, etc.) - Fully supported
- ✅ **Unity** - Should work (uses similar infrastructure)
- ⚠️ **KDE Plasma** - Lock/unlock commands work, but may require different screen locker integration
- ⚠️ **XFCE/LXDE** - May require custom lock/unlock commands
- ❌ **Wayland-only sessions** - Limited X11 command support

**Lock State Sync Feature:**
The automatic lock state synchronization (detecting system timeout locks) uses `loginctl` and works on any systemd-based distribution. This feature helps keep the internal state in sync when the desktop locks due to power-saving settings.

**Tested On:**
- Ubuntu 22.04+ with GNOME
- Should work on most modern systemd-based Linux distributions with BlueZ

## Building

To build the application, run:

```bash
make
```

This will produce the `BlueProximity` executable.

## Permissions

To access the Bluetooth hardware and read RSSI values without running as root, you must grant the binary the necessary capabilities:

```bash
sudo setcap 'cap_net_raw,cap_net_admin+eip' BlueProximity
```

Alternatively, you can run the application with `sudo`.

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

## Technical Debt / Unresolved

### Wayland Support

**Current Status:** Limited - relies on X11 compatibility layer

**The Problem:**
Wayland's security model is fundamentally different from X11. Key challenges:

1. **No Direct Display Access**: Wayland doesn't allow arbitrary processes to access the display server directly
2. **Compositor-Specific Protocols**: Each compositor (GNOME Shell, KWin, wlroots-based) has different protocols
3. **Screen Locking**: No standardized way to lock/unlock screens across compositors
4. **DPMS Control**: Power management requires compositor-specific extensions

**Potential Solutions:**

1. **Compositor-Specific Implementations**
   - GNOME/Mutter: Use `gdbus` to communicate with org.gnome.ScreenSaver
   - KDE/KWin: Use `qdbus` for org.freedesktop.ScreenSaver
   - wlroots (Sway): Use `swayidle` integration or IPC protocol
   - Requires detecting compositor and using appropriate method

2. **D-Bus Universal Approach** (Preferred)
   - Lock: `gdbus call --session --dest org.freedesktop.login1 --object-path /org/freedesktop/login1/session/self --method org.freedesktop.login1.Session.Lock`
   - Unlock: Similar with `.Unlock`
   - Already partially implemented via `loginctl` (which uses D-Bus under the hood)
   - DPMS/proximity: Compositor-specific, may need per-DE detection

3. **XDG Portal Integration** (Future-proof)
   - Use `org.freedesktop.portal.Inhibit` for preventing sleep
   - May not cover all use cases (screen lock/unlock)
   - Still evolving standard

**Implementation Plan:**
1. Detect session type (X11 vs Wayland) via `$XDG_SESSION_TYPE`
2. For Wayland sessions:
   - Use D-Bus directly instead of shell commands
   - Detect compositor (GNOME Shell, KWin, Sway, etc.)
   - Use compositor-specific protocols for DPMS/proximity commands
3. Maintain X11 fallback for compatibility
4. Add configuration option to override auto-detection

**Effort Estimate:** Medium - Requires D-Bus library integration and testing across multiple compositors

### COSMIC Desktop Support

**Current Status:** Untested

**Background:**
System76's COSMIC desktop (Rust-based, uses iced framework) is growing in popularity. Based on Wayland with custom compositor.

**Potential Approach:**
- COSMIC uses its own compositor (cosmic-comp)
- May support freedesktop D-Bus protocols
- Lock/unlock likely via `loginctl` (systemd-logind)
- DPMS/proximity may need COSMIC-specific implementation
- Monitor COSMIC development for standardized APIs

**Action Items:**
1. Wait for COSMIC 1.0 stable release
2. Test current implementation on COSMIC alpha/beta
3. Add COSMIC-specific detection if needed
4. Contribute to COSMIC protocols if extensions needed

**Effort Estimate:** Low-Medium - Likely works with current loginctl approach, may need minor tweaks
