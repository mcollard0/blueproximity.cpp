#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <cerrno>
#include <iomanip>
#include <algorithm>

// EIR/AD Data Types
#define EIR_FLAGS                   0x01
#define EIR_NAME_SHORT              0x08
#define EIR_NAME_COMPLETE           0x09
#define EIR_TX_POWER                0x0A

struct DeviceData {
    std::string mac;
    std::string name;
    int rssi;
    int tx_power;
    bool has_tx_power;
};

// Function to parse the advertising data
void parse_ad_data(uint8_t *data, size_t len, DeviceData &device) {
    size_t pos = 0;
    while (pos < len) {
        uint8_t length = data[pos];
        if (length == 0) break;
        
        if (pos + 1 + length > len) break; // Safety check
        
        uint8_t type = data[pos + 1];
        uint8_t *value = &data[pos + 2];
        uint8_t value_len = length - 1;
        
        switch (type) {
            case EIR_NAME_SHORT:
            case EIR_NAME_COMPLETE:
                if (device.name.empty() || type == EIR_NAME_COMPLETE) {
                    device.name.assign((char*)value, value_len);
                }
                break;
            case EIR_TX_POWER:
                if (value_len == 1) {
                    device.tx_power = (int8_t)value[0];
                    device.has_tx_power = true;
                }
                break;
        }
        
        pos += length + 1;
    }
}

int main() {
    int dev_id = hci_get_route(NULL);
    int sock = hci_open_dev(dev_id);
    if (dev_id < 0 || sock < 0) {
        perror("Error opening socket");
        return 1;
    }

    // Set scan parameters - Active scanning to get names (SCAN_REQ)
    // type=0x01 (Active), interval=0x10, window=0x10, own_type=0x00, filter=0x00
    if (hci_le_set_scan_parameters(sock, 0x01, 0x10, 0x10, 0x00, 0x00, 1000) < 0) {
        perror("Failed to set scan parameters");
        close(sock);
        return 1;
    }

    // Enable scanning
    // enable=1, filter_dup=0 (show all packets to update RSSI)
    if (hci_le_set_scan_enable(sock, 0x01, 0, 1000) < 0) {
        perror("Failed to enable scanning");
        close(sock);
        return 1;
    }

    std::cout << "Scanning for BLE devices... (Press Ctrl+C to stop)" << std::endl;
    std::cout << "Note: RSSI is the signal strength 'command' result for BLE." << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;
    std::cout << std::left << std::setw(20) << "MAC Address" 
              << std::setw(10) << "RSSI" 
              << std::setw(10) << "TX Power"
              << "Name" << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    unsigned char buf[HCI_MAX_EVENT_SIZE];
    struct hci_filter nf;
    
    // Set filter to catch LE Meta Events
    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);
    if (setsockopt(sock, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
        perror("setsockopt");
        return 1;
    }

    struct pollfd p;
    p.fd = sock;
    p.events = POLLIN;

    while (true) {
        int n = poll(&p, 1, 1000);
        if (n < 0) {
            if (errno == EINTR) break;
            perror("poll");
            break;
        }
        if (n == 0) continue;

        if (p.revents & POLLIN) {
            int len = read(sock, buf, sizeof(buf));
            if (len < 0) {
                if (errno == EAGAIN || errno == EINTR) continue;
                break;
            }

            evt_le_meta_event *meta = (evt_le_meta_event *)(buf + (1 + HCI_EVENT_HDR_SIZE));
            if (meta->subevent == EVT_LE_ADVERTISING_REPORT) {
                le_advertising_info *info = (le_advertising_info *)(meta->data + 1);
                int reports = meta->data[0];
                void *offset = meta->data + 1;
                
                for (int i=0; i<reports; i++) {
                    le_advertising_info *info_ptr = (le_advertising_info *)offset;
                    char addr[18];
                    ba2str(&info_ptr->bdaddr, addr);
                    
                    int8_t rssi = *(int8_t*)(info_ptr->data + info_ptr->length);
                    
                    DeviceData dev;
                    dev.mac = addr;
                    dev.rssi = (int)rssi;
                    dev.has_tx_power = false;
                    
                    parse_ad_data(info_ptr->data, info_ptr->length, dev);
                    
                    std::cout << std::left << std::setw(20) << dev.mac 
                              << std::setw(10) << dev.rssi 
                              << std::setw(10) << (dev.has_tx_power ? std::to_string(dev.tx_power) : "N/A")
                              << dev.name << std::endl;
                    
                    offset = (uint8_t*)info_ptr + 1 + 1 + 6 + 1 + info_ptr->length + 1;
                }
            }
        }
    }

    // Disable scanning
    hci_le_set_scan_enable(sock, 0x00, 1, 1000);
    close(sock);
    return 0;
}
