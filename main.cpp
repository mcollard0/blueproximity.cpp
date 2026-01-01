#include "BlueProximity.hpp"
#include "ConfigFile.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <getopt.h>
#include <unistd.h>
#include <cstdlib>
#include <pwd.h>
#include <iomanip>
#include <sys/wait.h>
#include <ctime>
#include <array>
#include <sstream>

std::string get_config_path() {
    const char* home = getenv( "HOME" );
    if ( !home ) {
        struct passwd* pw = getpwuid( getuid() );
        if ( pw ) home = pw->pw_dir;
    }
    return std::string( home ) + "/.blueproximity/config";
}

std::string exec_command_output( const char* cmd ) {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen( cmd, "r" );
    if ( !pipe ) return "";
    
    while ( fgets( buffer.data(), buffer.size(), pipe ) != nullptr ) {
        result += buffer.data();
    }
    pclose( pipe );
    
    // Trim trailing newline/whitespace
    while ( !result.empty() && ( result.back() == '\n' || result.back() == '\r' || result.back() == ' ' ) ) {
        result.pop_back();
    }
    return result;
}

struct SessionInfo {
    std::string username;
    std::string session_id;
    bool valid;
};

SessionInfo get_session_info() {
    SessionInfo info;
    info.valid = false;
    
    // Get username
    const char* user = getenv( "USER" );
    if ( !user ) {
        struct passwd* pw = getpwuid( getuid() );
        if ( pw ) user = pw->pw_name;
    }
    if ( !user ) {
        std::cerr << "Warning: Could not determine username" << std::endl;
        return info;
    }
    info.username = user;
    
    // Get session ID
    std::string cmd = "loginctl list-sessions --no-legend | grep " + info.username + " | awk '{print $1}' | head -n1";
    info.session_id = exec_command_output( cmd.c_str() );
    
    if ( info.session_id.empty() ) {
        std::cerr << "Warning: Could not determine session ID for user " << info.username << std::endl;
        return info;
    }
    
    info.valid = true;
    std::cout << "[ SYSTEM ] Session info cached: user=" << info.username 
              << " session=" << info.session_id << std::endl;
    return info;
}

bool is_desktop_locked( const SessionInfo& session ) {
    if ( !session.valid ) return false; // Assume unlocked if we can't check
    
    std::string cmd = "loginctl show-session " + session.session_id + " -p LockedHint";
    std::string result = exec_command_output( cmd.c_str() );
    
    return ( result.find( "LockedHint=yes" ) != std::string::npos );
}

std::string detect_desktop_environment() {
    // Check XDG_CURRENT_DESKTOP first
    const char* xdg_desktop = getenv( "XDG_CURRENT_DESKTOP" );
    if ( xdg_desktop ) {
        std::string desktop( xdg_desktop );
        // Convert to lowercase for comparison
        for ( auto& c : desktop ) c = std::tolower( c );
        
        if ( desktop.find( "gnome" ) != std::string::npos ) return "gnome";
        if ( desktop.find( "kde" ) != std::string::npos ) return "kde";
        if ( desktop.find( "plasma" ) != std::string::npos ) return "kde";
        if ( desktop.find( "cosmic" ) != std::string::npos ) return "cosmic";
    }
    
    // Check DESKTOP_SESSION
    const char* session = getenv( "DESKTOP_SESSION" );
    if ( session ) {
        std::string sess( session );
        for ( auto& c : sess ) c = std::tolower( c );
        
        if ( sess.find( "gnome" ) != std::string::npos ) return "gnome";
        if ( sess.find( "kde" ) != std::string::npos ) return "kde";
        if ( sess.find( "plasma" ) != std::string::npos ) return "kde";
        if ( sess.find( "cosmic" ) != std::string::npos ) return "cosmic";
    }
    
    // Check for running processes
    std::string gnome_check = exec_command_output( "pgrep -x gnome-shell > /dev/null 2>&1 && echo gnome" );
    if ( !gnome_check.empty() ) return "gnome";
    
    std::string kde_check = exec_command_output( "pgrep -x plasmashell > /dev/null 2>&1 && echo kde" );
    if ( !kde_check.empty() ) return "kde";
    
    std::string cosmic_check = exec_command_output( "pgrep -x cosmic-comp > /dev/null 2>&1 && echo cosmic" );
    if ( !cosmic_check.empty() ) return "cosmic";
    
    return "unknown";
}

void setup_desktop_commands( ConfigFile::GlobalConfig& config ) {
    // Detect and cache DISPLAY if not already set
    if ( config.display.empty() ) {
        const char* display_env = getenv( "DISPLAY" );
        if ( display_env ) {
            config.display = display_env;
            std::cout << "[ SYSTEM ] Detected DISPLAY: " << config.display << std::endl;
        } else {
            // Try to detect from active sessions
            std::string detected_display = exec_command_output( "w -h | grep -m1 tty | awk '{print $3}'" );
            if ( detected_display.empty() || detected_display == "-" ) {
                detected_display = ":0"; // Default fallback
            }
            config.display = detected_display;
            std::cout << "[ SYSTEM ] DISPLAY not set, using detected/default: " << config.display << std::endl;
        }
    } else {
        std::cout << "[ SYSTEM ] Using configured DISPLAY: " << config.display << std::endl;
    }
    
    // Detect and cache XAUTHORITY if not already set
    if ( config.xauthority.empty() ) {
        const char* xauth_env = getenv( "XAUTHORITY" );
        if ( xauth_env ) {
            config.xauthority = xauth_env;
            std::cout << "[ SYSTEM ] Detected XAUTHORITY: " << config.xauthority << std::endl;
        } else {
            // Default to ~/.Xauthority
            const char* home = getenv( "HOME" );
            if ( !home ) {
                struct passwd* pw = getpwuid( getuid() );
                if ( pw ) home = pw->pw_dir;
            }
            if ( home ) {
                config.xauthority = std::string( home ) + "/.Xauthority";
                std::cout << "[ SYSTEM ] XAUTHORITY not set, using default: " << config.xauthority << std::endl;
            }
        }
    } else {
        std::cout << "[ SYSTEM ] Using configured XAUTHORITY: " << config.xauthority << std::endl;
    }
    
    // Only detect if not already set
    if ( config.desktop_environment.empty() ) {
        config.desktop_environment = detect_desktop_environment();
        std::cout << "[ SYSTEM ] Detected desktop environment: " << config.desktop_environment << std::endl;
    } else {
        std::cout << "[ SYSTEM ] Using configured desktop environment: " << config.desktop_environment << std::endl;
    }
    
    // Set default commands if not already set
    if ( config.lock_cmd.empty() || config.unlock_cmd.empty() || config.prox_cmd.empty() ) {
        if ( config.desktop_environment == "gnome" ) {
            if ( config.lock_cmd.empty() ) config.lock_cmd = "loginctl lock-session";
            if ( config.unlock_cmd.empty() ) config.unlock_cmd = "loginctl unlock-session";
            if ( config.prox_cmd.empty() ) config.prox_cmd = "xset dpms force on";
        } else if ( config.desktop_environment == "kde" ) {
            if ( config.lock_cmd.empty() ) config.lock_cmd = "loginctl lock-session";
            if ( config.unlock_cmd.empty() ) config.unlock_cmd = "loginctl unlock-session";
            if ( config.prox_cmd.empty() ) config.prox_cmd = "qdbus org.freedesktop.ScreenSaver /ScreenSaver org.freedesktop.ScreenSaver.SimulateUserActivity";
        } else if ( config.desktop_environment == "cosmic" ) {
            if ( config.lock_cmd.empty() ) config.lock_cmd = "loginctl lock-session";
            if ( config.unlock_cmd.empty() ) config.unlock_cmd = "loginctl unlock-session";
            if ( config.prox_cmd.empty() ) config.prox_cmd = "loginctl unlock-session"; // Keep screen awake by simulating activity
        } else {
            // Unknown/custom DE - use loginctl as fallback
            if ( config.lock_cmd.empty() ) config.lock_cmd = "loginctl lock-session";
            if ( config.unlock_cmd.empty() ) config.unlock_cmd = "loginctl unlock-session";
            if ( config.prox_cmd.empty() ) config.prox_cmd = "xset dpms force on";
        }
    }
}

void print_help(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "Options:\n"
              << "  -m, --mac, --btmac <address> Bluetooth MAC address (can be specified multiple times)\n"
              << "  --blemac <address>           Bluetooth Low Energy MAC address (can be specified multiple times)\n"
              << "  -c, --channel <channel>      RFCOMM channel (default: 1, ignored for BLE)\n"
              << "  --lock-distance <dist>       Distance to lock (default: 7)\n"
              << "  --unlock-distance <dist>     Distance to unlock (default: 4)\n"
              << "  --lock-duration <secs>       Duration to lock (default: 6)\n"
              << "  --unlock-duration <secs>     Duration to unlock (default: 1)\n"
              << "  --lock-cmd <command>         Command to lock screen\n"
              << "  --unlock-cmd <command>       Command to unlock screen\n"
              << "  --prox-cmd <command>         Command to run when in proximity\n"
              << "  --prox-interval <secs>       Interval for proximity command (default: 60)\n"
              << "  --buffer-size <size>         RSSI buffer size (default: 1)\n"
              << "  -d, --debug                  Enable debug output (AT commands)\n"
              << "  -h, --help                   Show this help message\n";
}

void execute_command( const std::string& cmd, const std::string& display = "", const std::string& xauthority = "" ) {
    if ( cmd.empty() ) return;
    std::cout << "[ SYSTEM ] Executing: " << cmd << std::endl;
    
    std::string full_cmd = cmd;
    if ( !display.empty() ) {
        full_cmd = "DISPLAY=" + display + " ";
        if ( !xauthority.empty() ) {
            full_cmd += "XAUTHORITY=" + xauthority + " ";
        }
        full_cmd += cmd;
    }
    
    std::string bg_cmd = full_cmd + " &";
    int ret = system( bg_cmd.c_str() );
    if ( ret == -1 ) {
        std::cerr << "Error: system() call failed (fork failure)" << std::endl;
    } else {
        if ( WIFEXITED( ret ) ) {
            int exit_status = WEXITSTATUS( ret );
            if ( exit_status != 0 ) {
                std::cerr << "Warning: Command launch shell returned non-zero: " << exit_status << std::endl;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    std::string config_path = get_config_path();
    ConfigFile::GlobalConfig config = ConfigFile::load(config_path);
    
    // Setup desktop environment and default commands
    setup_desktop_commands( config );
    
    bool config_changed = false;
    std::vector<BlueProximity::Config> cmd_devices;

    BlueProximity::Config base_config;
    base_config.lock_distance = config.lock_distance;
    base_config.unlock_distance = config.unlock_distance;
    base_config.lock_duration = config.lock_duration;
    base_config.unlock_duration = config.unlock_duration;
    base_config.lock_command = config.lock_cmd;
    base_config.unlock_command = config.unlock_cmd;
    base_config.proximity_command = config.prox_cmd;
    base_config.proximity_interval = config.prox_interval;
    base_config.buffer_size = config.buffer_size;
    base_config.debug = config.debug;

    static struct option long_options[] = {
        {"mac",             required_argument, 0, 'm'},
        {"btmac",           required_argument, 0, 'm'},
        {"blemac",          required_argument, 0, 'M'},
        {"channel",         required_argument, 0, 'c'},
        {"lock-distance",   required_argument, 0, 'L'},
        {"unlock-distance", required_argument, 0, 'U'},
        {"lock-duration",   required_argument, 0, 'l'},
        {"unlock-duration", required_argument, 0, 'u'},
        {"lock-cmd",        required_argument, 0, '1'},
        {"unlock-cmd",      required_argument, 0, '2'},
        {"prox-cmd",        required_argument, 0, '3'},
        {"prox-interval",   required_argument, 0, 'i'},
        {"buffer-size",     required_argument, 0, 'b'},
        {"debug",           no_argument,       0, 'd'},
        {"help",            no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "m:M:c:h:d", long_options, &long_index)) != -1) {
        switch (opt) {
            case 'm': {
                std::cout << "Adding Classic Device (CLI): " << optarg << std::endl;
                BlueProximity::Config cfg = base_config;
                cfg.mac_address = optarg;
                cfg.is_ble = false;
                cmd_devices.push_back(cfg);
                config_changed = true;
                break;
            }
            case 'M': {
                std::cout << "Adding BLE Device (CLI): " << optarg << std::endl;
                BlueProximity::Config cfg = base_config;
                cfg.mac_address = optarg;
                cfg.is_ble = true;
                cmd_devices.push_back(cfg);
                config_changed = true;
                break;
            }
            case 'c': base_config.channel = std::atoi(optarg); config.devices.clear(); break; 
            case 'L': base_config.lock_distance = config.lock_distance = std::atoi(optarg); config_changed = true; break;
            case 'U': base_config.unlock_distance = config.unlock_distance = std::atoi(optarg); config_changed = true; break;
            case 'l': base_config.lock_duration = config.lock_duration = std::atoi(optarg); config_changed = true; break;
            case 'u': base_config.unlock_duration = config.unlock_duration = std::atoi(optarg); config_changed = true; break;
            case '1': base_config.lock_command = config.lock_cmd = optarg; config_changed = true; break;
            case '2': base_config.unlock_command = config.unlock_cmd = optarg; config_changed = true; break;
            case '3': base_config.proximity_command = config.prox_cmd = optarg; config_changed = true; break;
            case 'i': base_config.proximity_interval = config.prox_interval = std::atoi(optarg); config_changed = true; break;
            case 'b': base_config.buffer_size = config.buffer_size = std::atoi(optarg); config_changed = true; break;
            case 'd': base_config.debug = config.debug = true; config_changed = true; break;
            case 'h': print_help(argv[0]); return 0;
            default: print_help(argv[0]); return 1;
        }
    }
    
    std::vector<BlueProximity*> monitors;
    size_t max_name_len = 0;
    auto update_len = [&](const std::string& name, const std::string& mac) {
        size_t len = name.empty() ? mac.length() : name.length();
        if (len > max_name_len) max_name_len = len;
    };

    if (cmd_devices.empty()) {
        if (config.devices.empty()) {
            std::cout << "No devices configured. Scanning..." << std::endl;
            auto scanned = BlueProximity::scan_devices();
            if (scanned.empty()) {
                std::cerr << "No devices found during scan." << std::endl;
                return 1;
            }
            std::cout << "Found devices:\n";
            for (size_t i = 0; i < scanned.size(); ++i) {
                std::cout << i + 1 << ". " << scanned[i].name << " (" << scanned[i].mac << ")\n";
            }
            std::cout << "Enter number for Primary device (0 to skip): ";
            int choice;
            if (std::cin >> choice && choice > 0 && choice <= (int)scanned.size()) {
                ConfigFile::DeviceConfig dc;
                dc.mac = scanned[choice-1].mac;
                dc.name = scanned[choice-1].name;
                dc.is_ble = false; 
                dc.channel = 1; 
                config.devices.push_back(dc);
                config_changed = true;
            }
        }
        for (const auto& dev : config.devices) {
            update_len(dev.name, dev.mac);
        }
        for (const auto& dev : config.devices) {
            std::cout << "Loading Device (Config): " << (dev.name.empty() ? dev.mac : dev.name) << " (" << dev.mac << ")" << std::endl;
            BlueProximity::Config cfg = base_config;
            cfg.mac_address = dev.mac;
            cfg.name = dev.name;
            cfg.is_ble = dev.is_ble;
            cfg.channel = dev.channel;
            cfg.name_padding = max_name_len;
            monitors.push_back(new BlueProximity(cfg));
        }
    } else {
        config.devices.clear(); 
        for (const auto& cfg : cmd_devices) {
            update_len(cfg.name, cfg.mac_address);
        }
        for (const auto& cfg : cmd_devices) {
            BlueProximity::Config final_cfg = cfg;
            final_cfg.debug = base_config.debug; // Force debug update from base_config
            final_cfg.name_padding = max_name_len;
            monitors.push_back(new BlueProximity(final_cfg));
            
            ConfigFile::DeviceConfig dc;
            dc.mac = cfg.mac_address;
            dc.name = cfg.name;
            dc.is_ble = cfg.is_ble;
            dc.channel = cfg.channel;
            config.devices.push_back(dc);
        }
        config_changed = true; 
    }

    if (monitors.empty()) {
        std::cerr << "Error: No devices configured or selected.\n";
        return 1;
    }
    
    if (config_changed) {
        std::string dir = config_path.substr(0, config_path.find_last_of('/'));
        std::string cmd = "mkdir -p " + dir;
        int ret = system(cmd.c_str());
        if (ret != 0) std::cerr << "Warning: Failed to create config directory.\n";
        std::cout << "Saving configuration to " << config_path << std::endl;
        ConfigFile::save(config_path, config);
    }
    
    // Cache session info at startup
    SessionInfo session = get_session_info();
    if ( !session.valid ) {
        std::cerr << "Warning: Session info unavailable. Lock state sync will be disabled." << std::endl;
    }
    
    std::cout << "Starting monitoring loop..." << std::endl;

    enum State { GONE, ACTIVE };
    State current_state = GONE;
    int duration_count = 0;
    time_t last_prox_time = 0;
    time_t last_lock_check = 0;
    const int LOCK_CHECK_INTERVAL = 30; // Check lock state every 30 seconds

    int lock_threshold = -config.lock_distance;
    int unlock_threshold = -config.unlock_distance;

    while ( true ) {
        time_t now = time( NULL );
        double best_avg_rssi = -255.0;
        
        // Update all monitors and find best signal FIRST
        for ( auto* monitor : monitors ) {
            monitor->update(); // prints status
            double avg = monitor->get_average_rssi();
            if ( avg > best_avg_rssi ) {
                best_avg_rssi = avg;
            }
        }
        
        // Periodically check actual desktop lock state to sync with system
        if ( session.valid && ( now - last_lock_check >= LOCK_CHECK_INTERVAL ) ) {
            bool desktop_locked = is_desktop_locked( session );
            State expected_state = desktop_locked ? GONE : ACTIVE;
            
            if ( current_state != expected_state ) {
                std::cout << "[ SYSTEM ] Desktop lock state mismatch detected. ";
                std::cout << "Desktop is " << ( desktop_locked ? "LOCKED" : "UNLOCKED" );
                std::cout << ", internal state was " << ( current_state == ACTIVE ? "ACTIVE" : "GONE" );
                std::cout << ". Syncing..." << std::endl;
                
                // Smart duration_count handling based on RSSI and transition direction
                if ( current_state == ACTIVE && expected_state == GONE ) {
                    // Desktop locked externally while we thought it was unlocked
                    // If RSSI is good (above unlock threshold), start unlock counter at 1
                    if ( best_avg_rssi >= unlock_threshold && best_avg_rssi != -255.0 ) {
                        duration_count = 1;
                        std::cout << "[ SYSTEM ] RSSI is good (" << best_avg_rssi 
                                  << "), starting unlock counter at 1" << std::endl;
                    } else {
                        duration_count = 0;
                    }
                } else {
                    // Any other transition: reset to 0
                    duration_count = 0;
                }
                
                current_state = expected_state;
            }
            last_lock_check = now;
        }

        // Global State Machine Logic
        int required_duration = ( current_state == ACTIVE ) ? config.lock_duration : config.unlock_duration;
        
        if ( current_state == ACTIVE ) {
            // Check if we should lock
            if ( best_avg_rssi <= lock_threshold ) {
                duration_count++;
                if ( duration_count >= config.lock_duration ) {
                    std::cout << "[ SYSTEM ] Transitioning to GONE (Locking)" << std::endl;
                    current_state = GONE;
                    execute_command( config.lock_cmd, config.display, config.xauthority );
                    duration_count = 0;
                }
            } else {
                duration_count = 0; // Reset if any signal is good
            }
        } else {
            // Check if we should unlock
            if ( best_avg_rssi >= unlock_threshold && best_avg_rssi != -255.0 ) {
                duration_count++;
                if ( duration_count >= config.unlock_duration ) {
                    std::cout << "[ SYSTEM ] Transitioning to ACTIVE (Unlocking)" << std::endl;
                    current_state = ACTIVE;
                    execute_command( config.unlock_cmd, config.display, config.xauthority );
                    duration_count = 0;
                }
            } else {
                duration_count = 0;
            }
        }

        // Proximity Command
        if ( current_state == ACTIVE && !config.prox_cmd.empty() ) {
            if ( now - last_prox_time >= config.prox_interval ) {
                execute_command( config.prox_cmd, config.display, config.xauthority );
                last_prox_time = now;
            }
        }

        // Display Aggregated Status
        std::cout << "[ SYSTEM        ] Best Avg RSSI: " << std::fixed << std::setprecision( 1 ) << std::setw( 5 ) << best_avg_rssi 
                  << " Conf: " << duration_count << "/" << required_duration
                  << " State: " << ( current_state == ACTIVE ? "ACTIVE" : "GONE" ) << std::endl;
        std::cout << "------------------------------------------------------------" << std::endl;

        sleep( 1 ); 
    }

    for ( auto* monitor : monitors ) {
        delete monitor;
    }

    return 0;
}
