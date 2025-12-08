/*
 * rfcomm_server.c - Bluetooth RFCOMM server with debug logs
 * 
 * Compile: gcc -o rfcomm_server rfcomm_server.c -lbluetooth
 * Usage:   sudo ./rfcomm_server -e -x
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>

#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

/* Telemetry data structure */
typedef struct {
    uint8_t speed;           // B0: 0-255 (rpm/46)
    uint8_t throttle;        // B1: 0-255
    uint16_t total_miles;    // B2-B3: 16-bit odometer
    uint8_t battery;         // B4[0-6]: 0-100%
    uint8_t night_mode;      // B4[7]: 0/1
    uint8_t engine_temp;     // B5[0-5]: 0-63 (offset -20°C)
    uint8_t turn_signal;     // B5[6-7]: 0=none, 1=right, 2=left, 3=hazard
    uint8_t battery_temp;    // B6[0-5]: 0-63
    uint8_t horn;            // B6[6]: 0/1
    uint8_t beam;            // B6[7]: 0/1
    uint8_t alert;           // B7[0-2]: 0-7
    uint8_t state;           // B7[3-4]: 1=N, 2=D, 3=P
    uint8_t mode;            // B7[5-6]: 1=ECON, 2=COMF, 3=SPORT
    uint8_t maps;            // B7[7]: 0/1
} telemetry_t;

static volatile int g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
    printf("\n[DEBUG] Shutting down server...\n");
}

static void print_hex(const uint8_t *data, size_t len) {
    printf("[HEX] ");
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

static void print_timestamp(void) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
    printf("[%s] ", buf);
}

static int parse_telemetry(const uint8_t *data, size_t len, telemetry_t *telem) {
    // Expected frame: 0xCE, len(8), data[8], '\n' = 11 bytes
    if (len < 11) {
        return -1;  // incomplete frame
    }
    
    if (data[0] != 0xCE) {
        return -2;  // invalid start marker
    }
    
    if (data[1] != 8) {
        return -3;  // invalid length
    }
    
    if (data[10] != '\n') {
        return -4;  // missing delimiter
    }
    
    // Parse payload
    telem->speed = data[2];
    telem->throttle = data[3];
    telem->total_miles = (uint16_t)data[4] | ((uint16_t)data[5] << 8);
    telem->battery = data[6] & 0x7F;
    telem->night_mode = (data[6] >> 7) & 0x01;
    telem->engine_temp = data[7] & 0x3F;
    telem->turn_signal = (data[7] >> 6) & 0x03;
    telem->battery_temp = data[8] & 0x3F;
    telem->horn = (data[8] >> 6) & 0x01;
    telem->beam = (data[8] >> 7) & 0x01;
    telem->alert = data[9] & 0x07;
    telem->state = (data[9] >> 3) & 0x03;
    telem->mode = (data[9] >> 5) & 0x03;
    telem->maps = (data[9] >> 7) & 0x01;
    
    return 0;
}

static void print_telemetry(const telemetry_t *telem) {
    const char *state_str[] = {"", "N", "D", "P"};
    const char *mode_str[] = {"", "ECON", "COMF", "SPORT"};
    const char *signal_str[] = {"none", "right", "left", "hazard"};
    
    printf("\n╔══════════════════════════════════════════════════════════╗\n");
    printf("║                 TELEMETRY DATA RECEIVED                  ║\n");
    printf("╠══════════════════════════════════════════════════════════╣\n");
    printf("║ Speed:          %3d (RPM: ~%d)                          ║\n", 
           telem->speed, telem->speed * 46);
    printf("║ Throttle:       %3d                                      ║\n", 
           telem->throttle);
    printf("║ Odometer:       %5d miles                               ║\n", 
           telem->total_miles);
    printf("║ Battery:        %3d%%                                    ║\n", 
           telem->battery);
    printf("║ Engine Temp:    %3d (display: %d°C)                     ║\n", 
           telem->engine_temp, (int)telem->engine_temp - 20);
    printf("║ Battery Temp:   %3d                                      ║\n", 
           telem->battery_temp);
    printf("║ State:          %s                                       ║\n", 
           state_str[telem->state]);
    printf("║ Mode:           %-5s                                    ║\n", 
           mode_str[telem->mode]);
    printf("║ Turn Signal:    %-6s                                   ║\n", 
           signal_str[telem->turn_signal]);
    printf("║ Night Mode:     %s                                       ║\n", 
           telem->night_mode ? "ON " : "OFF");
    printf("║ High Beam:      %s                                       ║\n", 
           telem->beam ? "ON " : "OFF");
    printf("║ Horn:           %s                                       ║\n", 
           telem->horn ? "ON " : "OFF");
    printf("║ Alert:          %d                                        ║\n", 
           telem->alert);
    printf("║ Maps Switch:    %s                                       ║\n", 
           telem->maps ? "ON " : "OFF");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
}

static void handle_client(int client_sock, const char *client_addr, int echo_mode, int hex_mode) {
    uint8_t buf[1024];
    ssize_t bytes_read;
    unsigned long total_bytes = 0;
    unsigned long msg_count = 0;
    telemetry_t telem;

    printf("[INFO] Handling client %s\n", client_addr);

    while (g_running) {
        printf("[DEBUG] Waiting for data from client...\n");
        bytes_read = recv(client_sock, buf, sizeof(buf) - 1, 0);

        if (bytes_read < 0) {
            if (errno == EINTR) continue;
            perror("[ERROR] recv");
            break;
        }

        if (bytes_read == 0) {
            printf("[INFO] Client %s disconnected\n", client_addr);
            break;
        }

        total_bytes += bytes_read;
        msg_count++;

        print_timestamp();
        printf("[RX] %zd bytes from %s\n", bytes_read, client_addr);

        if (hex_mode) {
            print_hex(buf, bytes_read);
        }

        // Try to parse as telemetry data
        int parse_result = parse_telemetry(buf, bytes_read, &telem);
        if (parse_result == 0) {
            print_telemetry(&telem);
        } else {
            printf("[WARN] Failed to parse telemetry (code: %d)\n", parse_result);
            if (!hex_mode) {
                // Show hex if not already shown
                print_hex(buf, bytes_read);
            }
            // Try to show as text if printable
            buf[bytes_read] = '\0';
            int printable = 1;
            for (ssize_t i = 0; i < bytes_read; i++) {
                if (buf[i] < 32 && buf[i] != '\n' && buf[i] != '\r' && buf[i] != '\t') {
                    printable = 0;
                    break;
                }
            }
            if (printable) {
                printf("[TEXT] %s", buf);
                if (buf[bytes_read - 1] != '\n') printf("\n");
            }
        }

        if (echo_mode) {
            ssize_t sent = send(client_sock, buf, bytes_read, 0);
            if (sent < 0) {
                perror("[ERROR] send");
                break;
            }
            print_timestamp();
            printf("[TX] Echoed %zd bytes\n", sent);
        }
    }

    printf("[INFO] Client %s session ended. Total: %lu bytes, %lu messages\n",
           client_addr, total_bytes, msg_count);
}

int main(int argc, char **argv) {
    uint8_t channel = 1;
    int echo_mode = 0;
    int hex_mode = 0;
    int server_sock, client_sock;
    struct sockaddr_rc loc_addr = {0}, rem_addr = {0};
    socklen_t opt = sizeof(rem_addr);
    char client_addr[18];

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--echo") == 0) {
            echo_mode = 1;
        } else if (strcmp(argv[i], "-x") == 0 || strcmp(argv[i], "--hex") == 0) {
            hex_mode = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [-e|--echo] [-x|--hex] [channel]\n", argv[0]);
            return 0;
        } else {
            int ch = atoi(argv[i]);
            if (ch > 0 && ch < 31) channel = (uint8_t)ch;
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("[DEBUG] Creating RFCOMM socket...\n");
    server_sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (server_sock < 0) {
        perror("[ERROR] socket");
        return 1;
    }

    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = channel;

    printf("[DEBUG] Binding socket to channel %d...\n", channel);
    if (bind(server_sock, (struct sockaddr *)&loc_addr, sizeof(loc_addr)) < 0) {
        perror("[ERROR] bind");
        close(server_sock);
        return 1;
    }

    printf("[DEBUG] Listening for connections...\n");
    if (listen(server_sock, 1) < 0) {
        perror("[ERROR] listen");
        close(server_sock);
        return 1;
    }

    printf("==========================================\n");
    printf("  RFCOMM Server\n");
    printf("==========================================\n");
    printf("  MAC:        74:70:FD:0D:CA:45\n");
    printf("  Channel:    %d\n", channel);
    printf("  Echo mode:  %s\n", echo_mode ? "ON" : "OFF");
    printf("  Hex output: %s\n", hex_mode ? "ON" : "OFF");
    printf("==========================================\n");
    printf("[INFO] Waiting for connections...\n\n");

    while (g_running) {
        printf("[DEBUG] Waiting for accept()...\n");
        client_sock = accept(server_sock, (struct sockaddr *)&rem_addr, &opt);
        if (client_sock < 0) {
            if (errno == EINTR) continue;
            perror("[ERROR] accept");
            continue;
        }

        ba2str(&rem_addr.rc_bdaddr, client_addr);
        print_timestamp();
        printf("[INFO] Client connected: %s\n", client_addr);

        handle_client(client_sock, client_addr, echo_mode, hex_mode);

        close(client_sock);
        printf("[INFO] Waiting for next connection...\n\n");
    }

    close(server_sock);
    printf("[INFO] Server shut down.\n");

    return 0;
}

