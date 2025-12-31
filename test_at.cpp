#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <poll.h>
#include <algorithm>

int main() {
    std::string mac = "24:24:B7:9F:1B:47";
    int channel = 4;
    
    std::cout << "Connecting to " << mac << " on channel " << channel << "..." << std::endl;

    int sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_rc addr = { 0 };
    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = (uint8_t) channel;
    str2ba(mac.c_str(), &addr.rc_bdaddr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    std::cout << "Connected. Sending AT+RSSI?..." << std::endl;

    // Send AT+RSSI?
    const char* cmd = "\r\nAT+RSSI?\r"; // Try with CR LF prefix just in case, standard is AT...\r
    if (write(sock, cmd, strlen(cmd)) < 0) {
        perror("write");
        close(sock);
        return 1;
    }

    // Read response
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLIN;

    // Wait up to 2 seconds for response
    int timeout_ms = 2000;
    while (true) {
        int ret = poll(&pfd, 1, timeout_ms);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            char buf[256];
            memset(buf, 0, sizeof(buf));
            ssize_t r = read(sock, buf, sizeof(buf) - 1);
            if (r > 0) {
                std::string response(buf);
                 // Clean up output for display
                std::string clean_resp = response;
                clean_resp.erase(std::remove(clean_resp.begin(), clean_resp.end(), '\r'), clean_resp.end());
                clean_resp.erase(std::remove(clean_resp.begin(), clean_resp.end(), '\n'), clean_resp.end());
                
                std::cout << "Received raw: " << std::endl;
                for(int i=0; i<r; i++) {
                     printf("%02X ", (unsigned char)buf[i]);
                }
                printf("\n");
                
                std::cout << "Received text: [" << clean_resp << "]" << std::endl;
                
                // If we get OK or ERROR, we can probably stop, but let's just listen a bit more in case it's split
                if (response.find("OK") != std::string::npos || response.find("ERROR") != std::string::npos) {
                     break;
                }
            } else {
                break;
            }
        } else {
            std::cout << "Timeout or no data." << std::endl;
            break;
        }
    }

    close(sock);
    return 0;
}
