#ifndef BLUEPROXIMITY_HPP
#define BLUEPROXIMITY_HPP

#include <string>
#include <vector>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>

struct DeviceInfo {
    std::string mac;
    std::string name;
};

class BlueProximity {
public:
    struct Config {
        std::string mac_address;
        std::string name;
        int channel = 1;
        int lock_distance = 7;
        int unlock_distance = 4;
        int lock_duration = 6;
        int unlock_duration = 1;
        std::string lock_command;
        std::string unlock_command;
        std::string proximity_command;
        int proximity_interval = 60;
        int buffer_size = 1;
        bool is_ble = false;
        bool debug = false;
        size_t name_padding = 0;
    };

    BlueProximity(Config config);
    ~BlueProximity();

    void update(); // Called periodically
    double get_average_rssi() const;
    bool is_ble_device() const;
    
    static std::vector<DeviceInfo> scan_devices();

private:
    Config config;
    int socket_fd;
    int hci_socket;
    int dev_id;
    
    std::vector<int> rssi_buffer;
    size_t rssi_buffer_pos;

    time_t last_keepalive_time;

    bool connect();
    void disconnect();
    int read_rssi(int& rssi_value);
    void send_keepalive();
    int get_hci_conn_handle(int dev_id, const char* addr);
};

#endif // BLUEPROXIMITY_HPP