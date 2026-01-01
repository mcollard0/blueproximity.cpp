#ifndef CONFIGFILE_HPP
#define CONFIGFILE_HPP

#include "BlueProximity.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>

// Simple config format:
// KEY=VALUE
// [DEVICE]
// mac=...
// type=...
// channel=...
// ...

class ConfigFile {
public:
    struct DeviceConfig {
        std::string mac;
        std::string name;
        bool is_ble;
        int channel;
        // Add other per-device overrides if needed
    };

    struct GlobalConfig {
        int lock_distance = 7;
        int unlock_distance = 4;
        int lock_duration = 6;
        int unlock_duration = 2;
        std::string lock_cmd;
        std::string unlock_cmd;
        std::string prox_cmd;
        int prox_interval = 60;
        int buffer_size = 1;
        bool debug = false;
        std::string desktop_environment; // "gnome", "kde", or custom
        std::string display; // X11 DISPLAY environment variable
        std::vector<DeviceConfig> devices;
    };

    static GlobalConfig load(const std::string& path);
    static void save(const std::string& path, const GlobalConfig& config);
};

#endif // CONFIGFILE_HPP