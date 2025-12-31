#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <cerrno>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <array>

using namespace std;

struct DeviceInfo {
    string mac;
    string name;
    int rssi;
    string type; // "BT" or "BLE"
    string services;
    string vendor;
};

map<string, DeviceInfo> devices;

// Helper to run shell command and get output
string exec(const char* cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return "popen failed";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

string get_vendor(string mac) {
    if (mac.length() < 8) return "";
    string prefix = mac.substr(0, 8);
    replace(prefix.begin(), prefix.end(), ':', '-');
    transform(prefix.begin(), prefix.end(), prefix.begin(), ::toupper);
    
    string cmd = "grep -i \"^" + prefix + ".*(hex)\" /usr/share/ieee-data/oui.txt";
    string output = exec(cmd.c_str());
    
    if (output.empty()) return "";
    
    size_t pos = output.find("(hex)");
    if (pos != string::npos) {
        string vendor = output.substr(pos + 5);
        // Trim whitespace
        size_t first = vendor.find_first_not_of(" \t");
        if (string::npos == first) return "";
        size_t last = vendor.find_last_not_of(" \t\n\r");
        return vendor.substr(first, (last - first + 1));
    }
    return "";
}

// BLE Scan
void scan_ble(int duration_sec) {
    int dev_id = hci_get_route(NULL);
    int sock = hci_open_dev(dev_id);
    if (dev_id < 0 || sock < 0) return;

    // Set scan parameters
    hci_le_set_scan_parameters(sock, 0x01, 0x10, 0x10, 0x00, 0x00, 1000);
    hci_le_set_scan_enable(sock, 0x01, 0, 1000);

    // Filter
    struct hci_filter nf;
    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);
    setsockopt(sock, SOL_HCI, HCI_FILTER, &nf, sizeof(nf));

    unsigned char buf[HCI_MAX_EVENT_SIZE];
    struct pollfd p;
    p.fd = sock;
    p.events = POLLIN;

    time_t start = time(NULL);
    while (time(NULL) - start < duration_sec) {
        int n = poll(&p, 1, 100);
        if (n > 0 && (p.revents & POLLIN)) {
            int len = read(sock, buf, sizeof(buf));
            if (len > 0) {
                evt_le_meta_event *meta = (evt_le_meta_event *)(buf + (1 + HCI_EVENT_HDR_SIZE));
                if (meta->subevent == EVT_LE_ADVERTISING_REPORT) {
                    le_advertising_info *info = (le_advertising_info *)(meta->data + 1);
                    int reports = meta->data[0];
                    void *offset = meta->data + 1;
                    
                    for (int i=0; i<reports; i++) {
                        le_advertising_info *info_ptr = (le_advertising_info *)offset;
                        char addr[18];
                        ba2str(&info_ptr->bdaddr, addr);
                        string mac(addr);
                        
                        int8_t rssi = *(int8_t*)(info_ptr->data + info_ptr->length);
                        
                        // Parse Name
                        string name = "[Unknown]";
                        uint8_t *data = info_ptr->data;
                        size_t data_len = info_ptr->length;
                        size_t pos = 0;
                        while (pos < data_len) {
                            uint8_t len = data[pos];
                            if (len == 0) break;
                            if (pos + 1 + len > data_len) break;
                            uint8_t type = data[pos + 1];
                            if (type == 0x08 || type == 0x09) { // Short or Complete Name
                                name.assign((char*)&data[pos+2], len-1);
                                break;
                            }
                            pos += len + 1;
                        }

                        if (devices.find(mac) == devices.end()) {
                            DeviceInfo d;
                            d.mac = mac;
                            d.name = name;
                            d.rssi = rssi;
                            d.type = "BLE";
                            d.vendor = get_vendor(mac);
                            devices[mac] = d;
                        } else {
                            // Update RSSI
                            devices[mac].rssi = rssi;
                            if (name != "[Unknown]") devices[mac].name = name;
                        }
                        
                        offset = (uint8_t*)info_ptr + 1 + 1 + 6 + 1 + info_ptr->length + 1;
                    }
                }
            }
        }
    }
    hci_le_set_scan_enable(sock, 0x00, 1, 1000);
    close(sock);
}

// Classic Scan
void scan_classic(int duration_sec) {
    int dev_id = hci_get_route(NULL);
    int sock = hci_open_dev(dev_id);
    if (dev_id < 0 || sock < 0) return;

    inquiry_info *ii = NULL;
    int max_rsp = 255;
    int flags = IREQ_CACHE_FLUSH;
    
    // We use standard inquiry, which is blocking, so duration_sec is approximated by 'len' parameter (1.28s units)
    int len = duration_sec; 
    if (len < 1) len = 1;
    if (len > 30) len = 30;

    ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));
    
    // Note: Standard inquiry doesn't give RSSI easily without setting mode.
    // We will just do a basic scan for discovery.
    int num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
    
    for (int i = 0; i < num_rsp; i++) {
        char addr[19] = {0};
        char name[248] = {0};
        ba2str(&(ii+i)->bdaddr, addr);
        string mac(addr);
        
        // Get Name
        if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), name, 0) < 0)
            strcpy(name, "[Unknown]");
            
        DeviceInfo d;
        d.mac = mac;
        d.name = name;
        d.rssi = 0; // N/A for standard inquiry
        d.type = "BT";
        d.vendor = get_vendor(mac);
        
        // Prefer BLE entry if exists (dual mode devices), or just overwrite/add?
        // Usually we treat them as separate unless we merge by MAC, but BT and BLE macs can be same or random.
        // We'll index by MAC.
        if (devices.find(mac) == devices.end()) {
            devices[mac] = d;
        } else {
             // If existing was BLE, keep it as BLE but maybe update name
             if (devices[mac].type == "BLE" && string(name) != "[Unknown]") {
                 devices[mac].name = name;
             }
        }
    }
    
    free(ii);
    close(sock);
}

void get_services() {
    cout << "\nScanning for services... this may take a minute or ten..." << endl;
    
    for (auto &pair : devices) {
        DeviceInfo &d = pair.second;
        cout << "Probing " << d.mac << " (" << d.type << ")..." << flush;
        
        if (d.type == "BT") {
            string cmd = "sdptool browse " + d.mac;
            string output = exec(cmd.c_str());
            
            // Grep for Service Names and Channels
            string summary = "";
            stringstream ss(output);
            string line;
            while(getline(ss, line)) {
                if (line.find("Service Name:") != string::npos) {
                    size_t pos = line.find(":");
                    if (pos != string::npos) summary += line.substr(pos+1) + "; ";
                }
                if (line.find("Channel:") != string::npos) {
                    size_t pos = line.find(":");
                    if (pos != string::npos) summary += "Ch" + line.substr(pos+1) + " ";
                }
            }
            d.services = summary;
        } else {
            // BLE
            string cmd = "gatttool -b " + d.mac + " --primary";
            // gatttool is tricky, sometimes hangs. add timeout?
            // "timeout 5s gatttool ..."
            cmd = "timeout 5s " + cmd;
            string output = exec(cmd.c_str());
            // Output format: attr handle = 0x0001, end grp handle = 0x0005, uuid: ...
            // Just extract UUIDs
            string summary = "";
            stringstream ss(output);
            string line;
            while(getline(ss, line)) {
                if (line.find("uuid:") != string::npos) {
                     size_t pos = line.find("uuid:");
                     string uuid = line.substr(pos+5);
                     // Clean up UUID
                     uuid.erase(remove(uuid.begin(), uuid.end(), ' '), uuid.end());
                     // Try to map common UUIDs? Too much work. Just show first 4 chars.
                     if (uuid.length() > 4) summary += uuid.substr(0, 4) + ".. ";
                     else summary += uuid + " ";
                }
            }
            d.services = summary;
        }
        cout << " Done." << endl;
    }
}

int main() {
    cout << "Starting Bluetooth Scan (BT + BLE)..." << endl;
    
    cout << "Scanning BLE (5s)..." << endl;
    scan_ble(5);
    
    cout << "Scanning Classic BT (5s)..." << endl;
    scan_classic(5); // len=5 * 1.28s approx 6.4s
    
    cout << "\nFound Devices:" << endl;
    cout << left << setw(20) << "MAC" << setw(30) << "Name" << setw(6) << "Type" << setw(6) << "RSSI" << " Vendor" << endl;
    cout << string(100, '-') << endl;
    
    for (const auto &pair : devices) {
        const DeviceInfo &d = pair.second;
        string vendor = d.vendor;
        if (vendor.length() > 30) vendor = vendor.substr(0, 27) + "...";
        cout << left << setw(20) << d.mac << setw(30) << d.name << setw(6) << d.type << setw(6) << d.rssi << " " << vendor << endl;
    }
    
    get_services();
    
    cout << "\nFinal Report:" << endl;
    cout << string(120, '-') << endl;
    cout << left << setw(18) << "MAC" << setw(20) << "Name" << setw(5) << "Type" << setw(25) << "Vendor" << " Services" << endl;
    cout << string(120, '-') << endl;
    
    for (const auto &pair : devices) {
        const DeviceInfo &d = pair.second;
        // Clean up newlines in services
        string clean_services = d.services;
        replace(clean_services.begin(), clean_services.end(), '\n', ' ');
        if (clean_services.length() > 55) clean_services = clean_services.substr(0, 52) + "...";
        
        string vendor = d.vendor;
        if (vendor.length() > 24) vendor = vendor.substr(0, 21) + "...";

        cout << left << setw(18) << d.mac << setw(20) << d.name.substr(0, 19) << setw(5) << d.type << setw(25) << vendor << " " << clean_services << endl;
    }

    return 0;
}
