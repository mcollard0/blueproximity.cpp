#include "ConfigFile.hpp"
#include <iostream>

ConfigFile::GlobalConfig ConfigFile::load(const std::string& path) {
    GlobalConfig config;
    // Set defaults
    config.lock_cmd = "loginctl lock-session";
    config.unlock_cmd = "loginctl unlock-session";
    config.prox_cmd = "xset dpms force on";

    std::ifstream file(path);
    if (!file.is_open()) return config;

    std::string line;
    DeviceConfig current_device;
    bool in_device = false;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line == "[DEVICE]") {
            if (in_device && !current_device.mac.empty()) {
                config.devices.push_back(current_device);
            }
            in_device = true;
            current_device = DeviceConfig();
            current_device.channel = 1;
            current_device.is_ble = false;
            continue;
        }

        std::stringstream ss(line);
        std::string key, val;
        std::getline(ss, key, '=');
        std::getline(ss, val);

        if (in_device) {
            if (key == "mac") current_device.mac = val;
            else if (key == "name") current_device.name = val;
            else if (key == "channel") current_device.channel = std::stoi(val);
            else if (key == "is_ble") current_device.is_ble = (val == "1" || val == "true");
        } else {
            if ( key == "lock_distance" ) config.lock_distance = std::stoi( val );
            else if ( key == "unlock_distance" ) config.unlock_distance = std::stoi( val );
            else if ( key == "lock_duration" ) config.lock_duration = std::stoi( val );
            else if ( key == "unlock_duration" ) config.unlock_duration = std::stoi( val );
            else if ( key == "lock_cmd" ) config.lock_cmd = val;
            else if ( key == "unlock_cmd" ) config.unlock_cmd = val;
            else if ( key == "prox_cmd" ) config.prox_cmd = val;
            else if ( key == "prox_interval" ) config.prox_interval = std::stoi( val );
            else if ( key == "buffer_size" ) config.buffer_size = std::stoi( val );
            else if ( key == "debug" ) config.debug = ( val == "1" || val == "true" );
            else if ( key == "desktop_environment" ) config.desktop_environment = val;
            else if ( key == "display" ) config.display = val;
            else if ( key == "xauthority" ) config.xauthority = val;
        }
    }
    if (in_device && !current_device.mac.empty()) {
        config.devices.push_back(current_device);
    }

    return config;
}

void ConfigFile::save( const std::string& path, const GlobalConfig& config ) {
    std::ofstream file( path );
    if ( !file.is_open() ) return;

    file << "lock_distance=" << config.lock_distance << "\n";
    file << "unlock_distance=" << config.unlock_distance << "\n";
    file << "lock_duration=" << config.lock_duration << "\n";
    file << "unlock_duration=" << config.unlock_duration << "\n";
    file << "lock_cmd=" << config.lock_cmd << "\n";
    file << "unlock_cmd=" << config.unlock_cmd << "\n";
    file << "prox_cmd=" << config.prox_cmd << "\n";
    file << "prox_interval=" << config.prox_interval << "\n";
    file << "buffer_size=" << config.buffer_size << "\n";
    file << "debug=" << ( config.debug ? "true" : "false" ) << "\n";
    if ( !config.desktop_environment.empty() ) {
        file << "desktop_environment=" << config.desktop_environment << "\n";
    }
    if ( !config.display.empty() ) {
        file << "display=" << config.display << "\n";
    }
    if ( !config.xauthority.empty() ) {
        file << "xauthority=" << config.xauthority << "\n";
    }

    for (const auto& dev : config.devices) {
        file << "\n[DEVICE]\n";
        file << "mac=" << dev.mac << "\n";
        file << "name=" << dev.name << "\n";
        file << "channel=" << dev.channel << "\n";
        file << "is_ble=" << (dev.is_ble ? "true" : "false") << "\n";
    }
}
