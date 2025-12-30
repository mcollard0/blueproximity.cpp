#include "BlueProximity.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <sys/poll.h>
#include <vector>
#include <iomanip>
#include <sys/wait.h>

#define EIR_FLAGS                   0x01  /* flags */
#define EIR_UUID16_SOME             0x02  /* 16-bit UUID, more available */
#define EIR_UUID16_ALL              0x03  /* 16-bit UUID, all listed */
#define EIR_UUID32_SOME             0x04  /* 32-bit UUID, more available */
#define EIR_UUID32_ALL              0x05  /* 32-bit UUID, all listed */
#define EIR_UUID128_SOME            0x06  /* 128-bit UUID, more available */
#define EIR_UUID128_ALL             0x07  /* 128-bit UUID, all listed */
#define EIR_NAME_SHORT              0x08  /* shortened local name */
#define EIR_NAME_COMPLETE           0x09  /* complete local name */
#define EIR_TX_POWER                0x0A  /* transmit power level */
#define EIR_DEVICE_ID               0x10  /* device ID */

std::vector<DeviceInfo> BlueProximity::scan_devices() {
    std::vector<DeviceInfo> devices;
    int dev_id = hci_get_route(NULL);
    int sock = hci_open_dev(dev_id);
    if (dev_id < 0 || sock < 0) {
        std::cerr << "Error opening socket for scanning." << std::endl;
        return devices;
    }

    int len = 8;
    int max_rsp = 255;
    int flags = IREQ_CACHE_FLUSH;
    inquiry_info *ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));
    
    std::cout << "Scanning for devices..." << std::endl;
    int num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
    if (num_rsp < 0) perror("hci_inquiry");

    for (int i = 0; i < num_rsp; i++) {
        char addr[19] = {0};
        char name[248] = {0};
        ba2str(&(ii+i)->bdaddr, addr);
        memset(name, 0, sizeof(name));
        if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), name, 0) < 0)
            strcpy(name, "[unknown]");
        
        devices.push_back({std::string(addr), std::string(name)});
    }

    free(ii);
    close(sock);
    return devices;
}

static int read_ble_rssi_scan(int hci_sock, const std::string& target_mac) {
    unsigned char buf[HCI_MAX_EVENT_SIZE];
    struct hci_filter nf, of;
    socklen_t olen;
    int len;
    
    // Set filter to catch LE Meta Events
    olen = sizeof(of);
    if (getsockopt(hci_sock, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
        return -255;
    }

    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);
    if (setsockopt(hci_sock, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
        return -255;
    }

    // Enable scanning
    // Scan parameters: type=0 (passive), interval=0x10, window=0x10, own_type=0, filter=0
    hci_le_set_scan_parameters(hci_sock, 0x00, 0x10, 0x10, 0x00, 0x00, 1000);
    hci_le_set_scan_enable(hci_sock, 0x01, 1, 1000);

    int found_rssi = -255;
    
    // Poll for 1 second
    struct pollfd p;
    p.fd = hci_sock;
    p.events = POLLIN;
    
    time_t start = time(NULL);
    while (time(NULL) - start < 2) { // up to 2 seconds
        int n = poll(&p, 1, 500); // 500ms timeout per poll
        if (n < 0) break; // Error
        if (n == 0) continue; // Timeout, check total time

        if ((p.revents & POLLIN)) {
            while ((len = read(hci_sock, buf, sizeof(buf))) < 0) {
                if (errno == EAGAIN || errno == EINTR) continue;
                break;
            }
            if (len < 0) break;

            evt_le_meta_event *meta = (evt_le_meta_event *)(buf + (1 + HCI_EVENT_HDR_SIZE));
            if (meta->subevent == EVT_LE_ADVERTISING_REPORT) {
                // Ensure length check
                // ...
                le_advertising_info *info = (le_advertising_info *)(meta->data + 1);
                char addr[18];
                ba2str(&info->bdaddr, addr);
                
                if (target_mac == addr) {
                    int reports = meta->data[0];
                    void *offset = meta->data + 1;
                    
                    for (int i=0; i<reports; i++) {
                        le_advertising_info *info_ptr = (le_advertising_info *)offset;
                        char current_addr[18];
                        ba2str(&info_ptr->bdaddr, current_addr);
                        
                        if (target_mac == current_addr) {
                            // RSSI is the byte after the data
                            int8_t rssi = *(int8_t*)(info_ptr->data + info_ptr->length);
                            found_rssi = (int)rssi;
                            goto cleanup;
                        }
                        
                        offset = (uint8_t*)info_ptr + 1 + 1 + 6 + 1 + info_ptr->length + 1;
                    }
                }
            }
        }
    }

cleanup:
    // Restore filter
    setsockopt(hci_sock, SOL_HCI, HCI_FILTER, &of, sizeof(of));
    // Disable scanning
    hci_le_set_scan_enable(hci_sock, 0x00, 1, 1000);
    
    return found_rssi;
}

BlueProximity::BlueProximity(Config config) : config(config), socket_fd(-1), hci_socket(-1), rssi_buffer_pos(0), last_keepalive_time(0) {
    if (config.buffer_size < 1) config.buffer_size = 1;
    rssi_buffer.resize(config.buffer_size, -255);
    
    // Open HCI socket for RSSI reading
    dev_id = hci_get_route(NULL);
    hci_socket = hci_open_dev(dev_id);
    if (hci_socket < 0) {
        std::cerr << "Failed to open HCI device" << std::endl;
    }
}

BlueProximity::~BlueProximity() {
    disconnect();
    if (hci_socket >= 0) {
        close(hci_socket);
    }
}

void BlueProximity::disconnect() {
    if (socket_fd >= 0) {
        close(socket_fd);
        socket_fd = -1;
    }
}

bool BlueProximity::connect() {
    if (socket_fd >= 0) return true;

    struct sockaddr_rc addr = { 0 };
    int status;

    // Allocate a socket
    socket_fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (socket_fd < 0) {
        perror("socket");
        return false;
    }

    // Set the connection parameters (who to connect to)
    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = (uint8_t) config.channel;
    str2ba(config.mac_address.c_str(), &addr.rc_bdaddr);

    // Connect to server
    status = ::connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr));

    if (status < 0) {
        if (config.debug) perror("connect"); 
        close(socket_fd);
        socket_fd = -1;
        return false;
    }
    
    return true;
}

int BlueProximity::get_hci_conn_handle(int dev_id, const char* addr) {
    struct hci_conn_list_req *cl;
    struct hci_conn_info *ci;
    int i;
    char bd_addr[18];
    
    if (dev_id < 0) return -1;

    cl = (struct hci_conn_list_req *)malloc(10 * sizeof(*ci) + sizeof(*cl));
    if (!cl) return -1;

    cl->dev_id = dev_id;
    cl->conn_num = 10;
    ci = cl->conn_info;

    if (ioctl(hci_socket, HCIGETCONNLIST, (void *)cl)) {
        if (config.debug) perror("HCIGETCONNLIST");
        free(cl);
        return -1;
    }

    for (i = 0; i < cl->conn_num; i++, ci++) {
        ba2str(&ci->bdaddr, bd_addr);
        if (strcmp(bd_addr, addr) == 0) {
            int handle = ci->handle;
            free(cl);
            return handle;
        }
    }

    if (config.debug) std::cerr << "Connection handle not found for " << addr << std::endl;
    free(cl);
    return -1;
}

int BlueProximity::read_rssi(int& rssi_value) {
    if (hci_socket < 0) return -1;

    // Get handle
    int handle = get_hci_conn_handle(dev_id, config.mac_address.c_str());
    if (handle < 0) {
        if (config.debug) std::cerr << "Failed to get HCI handle for " << config.mac_address << std::endl;
        return -1;
    }

    int8_t rssi;
    if (hci_read_rssi(hci_socket, handle, &rssi, 1000) < 0) {
        if (config.debug) perror("hci_read_rssi");
        return -1;
    }

    rssi_value = (int)rssi;
    return 0;
}

void BlueProximity::send_keepalive() {
    if (socket_fd < 0) return;
    
    // Send AT command (or just an empty carriage return) to keep link alive
    // "AT\r" is a standard modem command.
    const char* keepalive_cmd = "AT\r";
    
    if (config.debug) {
        std::cout << "[" << config.mac_address << "] Sending: " << "AT" << std::endl;
    }

    ssize_t written = write(socket_fd, keepalive_cmd, strlen(keepalive_cmd));
    
    if (written < 0) {
        if (config.debug) {
            std::cerr << "[" << config.mac_address << "] Keepalive write failed" << std::endl;
        }
    } else {
        // Read response
        // Use poll to avoid blocking indefinitely
        struct pollfd pfd;
        pfd.fd = socket_fd;
        pfd.events = POLLIN;
        
        // Wait up to 500ms for response
        int ret = poll(&pfd, 1, 500);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            char buf[256];
            memset(buf, 0, sizeof(buf));
            ssize_t r = read(socket_fd, buf, sizeof(buf) - 1);
            if (r > 0) {
                // Remove trailing newlines for cleaner output
                std::string response(buf);
                response.erase(std::remove(response.begin(), response.end(), '\r'), response.end());
                response.erase(std::remove(response.begin(), response.end(), '\n'), response.end());
                
                if (config.debug) {
                    std::cout << "[" << config.mac_address << "] Received: " << response << std::endl;
                }
            }
        }
    }
}

void BlueProximity::update() {
    int rssi = -255;
    
    if (config.is_ble) {
        // BLE Mode
        if (hci_socket >= 0) {
            rssi = read_ble_rssi_scan(hci_socket, config.mac_address);
        } else {
             // Try to reopen
            dev_id = hci_get_route(NULL);
            hci_socket = hci_open_dev(dev_id);
            if (hci_socket >= 0) {
                 rssi = read_ble_rssi_scan(hci_socket, config.mac_address);
            }
        }
    } else {
        // Classic Mode
        if (connect()) {
            if (read_rssi(rssi) < 0) {
                // Failed to read RSSI, maybe connection lost?
                // Wait for next cycle to reconnect
                disconnect();
                rssi = -255; 
            }
        } else {
            rssi = -255;
        }
    }

    // Update buffer
    rssi_buffer[rssi_buffer_pos] = rssi;
    rssi_buffer_pos = (rssi_buffer_pos + 1) % config.buffer_size;

    // Calculate average for display
    double sum = 0;
    int best_rssi = -255;
    for (int r : rssi_buffer) {
         sum += r;
         if (r > best_rssi) best_rssi = r;
    }
    double avg_rssi = sum / config.buffer_size;
    
    // Keep-alive (every 25 seconds) - Classic RFCOMM only
    if (!config.is_ble && socket_fd >= 0) {
        time_t now = time(NULL);
        if (now - last_keepalive_time >= 25) {
            send_keepalive();
            last_keepalive_time = now;
        }
    }

    std::cout << "[ " << std::left << std::setw(config.name_padding) << (config.name.empty() ? config.mac_address : config.name) 
              << " ] " << (config.is_ble ? "(BLE)" : "(BT) ") << " " << config.mac_address 
              << " RSSI: " << std::right << std::setw(4) << rssi 
              << " Best: " << std::setw(4) << best_rssi
              << " Avg: " << std::setw(6) << avg_rssi << std::endl;
}

double BlueProximity::get_average_rssi() const {
    double sum = 0;
    for (int r : rssi_buffer) {
         sum += r;
    }
    return sum / config.buffer_size;
}

bool BlueProximity::is_ble_device() const {
    return config.is_ble;
}
