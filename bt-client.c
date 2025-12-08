#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <fcntl.h>

/* ------------ Simulation State (incremental numbers) ------------- */
static volatile bool g_running = true;

static uint16_t rpm_vtl = 0;        // simulated motor RPM
static uint16_t voltage_vtl = 630;  // 630..830 ~ maps to 0..100%
static uint8_t  contemp_vtl = 25;   // 0..63 (offset -20 °C -> display)
static uint8_t  mode_vtl = 1;       // 1..3
static uint8_t  val_state = 1;      // 1=N,2=D,3=P (for packing)
static uint16_t miles_acc = 0;      // 0..65535 (16-bit)

static uint8_t  sein_left = 0;      // 0/1
static uint8_t  sein_right = 0;     // 0/1
static uint8_t  beams_on = 0;       // 0/1
static uint8_t  night_mode = 1;     // 0/1

/* ------------ Helpers ------------- */
static void handle_sigint(int sig) {
    (void)sig;
    g_running = false;
}

static uint8_t clamp_u8(int v, int lo, int hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (uint8_t)v;
}

/*  B0 SPEED: val_speed = rpm_vtl / 46  (0..255) */
static uint8_t show_speed(void) {
    return (uint8_t)(rpm_vtl / 46);
}

/*  B1 THROTTLE: not available -> simulate 0..255 sawtooth */
static uint8_t show_throt(void) {
    static uint8_t thr = 0;
    thr += 3; // wraps naturally
    return thr;
}

/* B2-B3 TOTAL DISTANCE: 16-bit, scale 0.5 (spec); we just send raw 16-bit counter */
static uint8_t show_miles_lsb(void) {
    return (uint8_t)(miles_acc & 0xFF);
}
static uint8_t show_miles_msb(void) {
    return (uint8_t)((miles_acc >> 8) & 0xFF);
}

/* B4(L) BATTERY: 0..100 computed from 630..830 -> (v-630)/2 */
static uint8_t show_battr(void) {
    int val = (voltage_vtl > 630) ? ((int)voltage_vtl - 630) / 2 : 0;
    if (val > 100) val = 100;
    return (uint8_t)val;
}

/* B4(H) NIGHT MODE: 1 bit at bit7 */
static uint8_t show_night(void) {
    return night_mode ? 1 : 0;
}

/* B5(L) ENGINE TEMP: 6-bit (0..63). We just return contemp_vtl limited to 6 bits. */
static uint8_t show_enginetemp(void) {
    return (uint8_t)(contemp_vtl & 0x3F);
}

/* B5(H) SEIN (turn signals): 2-bit at bit6-7 of byte5 half? In our packing we put it in the high 2 bits of B5 */
static uint8_t show_seinx(void) {
    if (sein_left && sein_right) return 3;  // hazard
    if (sein_right) return 1;               // right
    if (sein_left)  return 2;               // left
    return 0;                               
}

/* B6(L1) BATTERY TEMP: 6-bit (we simulate constant 0) */
static uint8_t show_battrtemp(void) {
    return 0;
}

/* B6(L2) HORN: 1 bit */
static uint8_t show_horns(void) {
    return 0;
}

/* B6(H) BEAM: 1 bit */
static uint8_t show_beams(void) {
    return beams_on ? 1 : 0;
}

/* B7(L1) ALERTS: 3-bit */
static uint8_t show_alert(void) {
    return 0;
}

/* B7(L2) STATE: 2-bit (1=N,2=D,3=P) */
static uint8_t show_state(void) {
    return val_state & 0x03;
}

/* B7(H1) MODE: 2-bit (1=ECON,2=COMF,3=SPORT) */
static uint8_t show_modes(void) {
    return mode_vtl & 0x03;
}

/* B7(H2) MAPS SWITCH: 1 bit; simulate 0 */
static uint8_t show_maps(void) {
    return 0;
}

/* ----- Pack 9-byte frame exactly like Arduino code intent ----- 
 * Byte layout (per your notes):
 * B0: speed (8)
 * B1: throttle (8)
 * B2: miles LSB (8)
 * B3: miles MSB (8)
 * B4: battery (7) | night<<7 (1)
 * B5: eng_temp (6) | sein (2)
 * B6: batt_temp (6) | horn<<6 (1) | beam<<7 (1)
 * B7: alert (3) | state<<3 (2) | mode<<5 (2) | maps<<7 (1)
 * B8: '\n'
 */
static void build_frame(uint8_t out[11]) {
    uint8_t speed  = show_speed();
    uint8_t throt  = show_throt();
    uint8_t m_lsb  = show_miles_lsb();
    uint8_t m_msb  = show_miles_msb();
    uint8_t battr  = show_battr();        // 0..100 (7 bits)
    uint8_t night  = show_night() & 0x01;

    uint8_t etemp  = show_enginetemp() & 0x3F;
    uint8_t sein   = show_seinx() & 0x03;

    uint8_t btemp  = show_battrtemp() & 0x3F;
    uint8_t horn   = show_horns() & 0x01;
    uint8_t beam   = show_beams() & 0x01;

    uint8_t alert  = show_alert() & 0x07;
    uint8_t state  = show_state() & 0x03;
    uint8_t mode   = show_modes() & 0x03;
    uint8_t maps   = show_maps() & 0x01;

    out[0] = 0xCE; // start byte
    out[1] = 8;    // length byte
    out[2] = speed;
    out[3] = throt;
    out[4] = m_lsb;
    out[5] = m_msb;
    out[6] = (uint8_t)((battr & 0x7F) | (night << 7));
    out[7] = (uint8_t)((etemp & 0x3F) | (sein  << 6));
    out[8] = (uint8_t)((btemp & 0x3F) | (horn  << 6) | (beam << 7));
    out[9] = (uint8_t)((alert & 0x07) | (state << 3) | (mode << 5) | (maps << 7));
    out[10] = '\n'; // delimiter
}

/* Increment and wrap the simulated signals to look "alive" */
static void simulate_tick(void) {
    rpm_vtl = (rpm_vtl + 50) % 12000;                 // 0..11950
    voltage_vtl = 630 + ((voltage_vtl - 630 + 1) % 201); // 630..830
    contemp_vtl = (contemp_vtl + 1) % 64;             // 0..63
    miles_acc   = (uint16_t)(miles_acc + 7);          // wraps 16-bit

    // mode cycles 1->2->3->1...
    static int mode_cnt = 0;
    mode_cnt = (mode_cnt + 1) % 30;
    if (mode_cnt == 0) {
        mode_vtl++;
        if (mode_vtl < 1 || mode_vtl > 3) mode_vtl = 1;
    }

    // state toggles: N->D->P->N...
    static int state_cnt = 0;
    state_cnt = (state_cnt + 1) % 50;
    if (state_cnt == 0) {
        val_state++;
        if (val_state < 1 || val_state > 3) val_state = 1;
    }

    // sein pattern: right, none, left, hazard, none...
    static int sein_phase = 0;
    sein_phase = (sein_phase + 1) % 80;
    if (sein_phase < 15) { sein_right = 1; sein_left = 0; }
    else if (sein_phase < 30) { sein_right = sein_left = 0; }
    else if (sein_phase < 45) { sein_right = 0; sein_left = 1; }
    else if (sein_phase < 60) { sein_right = sein_left = 1; }
    else { sein_right = sein_left = 0; }

    // beams on/off slowly
    static int beam_cnt = 0;
    beam_cnt = (beam_cnt + 1) % 40;
    if (beam_cnt == 0) beams_on = !beams_on;

    // night toggle very slow
    static int night_cnt = 0;
    night_cnt = (night_cnt + 1) % 200;
    if (night_cnt == 0) night_mode = !night_mode;
}

/* Sleep helper in milliseconds */
static void msleep(unsigned ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static uint64_t epoch_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 +
           (uint64_t)ts.tv_nsec / 1000000;
}

static int enable_sockopts(int s)
{
    int one = 1;

    // Keepalive untuk RFCOMM (lebih cepat deteksi mati)
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));

    return 0;
}

static int connect_blocking_with_timeout(int s, struct sockaddr_rc *addr)
{
    // Non-blocking
    fcntl(s, F_SETFL, O_NONBLOCK);

    int ret = connect(s, (struct sockaddr *)addr, sizeof(*addr));
    if (ret == 0) {
        return 0;
    }

    if (errno != EINPROGRESS) {
        return -1;
    }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(s, &wfds);

    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };

    ret = select(s + 1, NULL, &wfds, NULL, &tv);
    if (ret <= 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    int err = 0;
    socklen_t len = sizeof(err);
    getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len);

    if (err != 0) {
        errno = err;
        return -1;
    }

    return 0;
}

static int safe_reconnect(struct sockaddr_rc *addr)
{
    int s;

RETRY:
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (s < 0) {
        fprintf(stderr, "[ERR] socket: %s\n", strerror(errno));
        usleep(200 * 1000);
        goto RETRY;
    }

    enable_sockopts(s);

    if (connect_blocking_with_timeout(s, addr) == 0) {
        fprintf(stderr, "[INFO] Reconnected.\n");
        return s;
    }

    fprintf(stderr, "[WARN] connect failed: %s\n", strerror(errno));
    close(s);

    usleep(300 * 1000); // RFCOMM cooldown
    goto RETRY;
}

/* ---------- CLIENT ---------- */
static int run_client(const char *mac_addr,
                      uint8_t channel,
                      unsigned interval_ms,
                      bool verbose)
{
    struct sockaddr_rc addr = {0};
    int s;

    addr.rc_family = AF_BLUETOOTH;
    str2ba(mac_addr, &addr.rc_bdaddr);
    addr.rc_channel = channel;

    fprintf(stderr, "[INFO] Connecting to %s ch %u...\n",
            mac_addr, channel);

    s = safe_reconnect(&addr);

    uint64_t last_ping = 0;

    while (g_running) {
        uint8_t frame[11];
        simulate_tick();
        build_frame(frame);

        ssize_t w = send(s, frame, sizeof(frame), MSG_NOSIGNAL);

        if (w < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Socket belum siap → tunggu sejenak
                usleep(2000);
                continue;
            }

            fprintf(stderr, "[ERR] write: %s. Reconnecting...\n",
                    strerror(errno));
            close(s);
            s = safe_reconnect(&addr);
            continue;
        }

        if ((size_t)w < sizeof(frame)) {
            fprintf(stderr, "[ERR] Partial write. Reconnecting...\n");
            close(s);
            s = safe_reconnect(&addr);
            continue;
        }

        if (verbose) {
            fprintf(stderr, "TX:");
            for (int i = 0; i < 9; i++)
                fprintf(stderr, " %u", frame[i]);
            fprintf(stderr, "\n");
        }

        uint64_t now = epoch_ms();
        if (now - last_ping > 3000) {
            char ping = 0xFF;

            ssize_t p = send(s, &ping, 1, MSG_NOSIGNAL);
            if (p < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "[ERR] silent disconnect.\n");
                close(s);
                s = safe_reconnect(&addr);
            }

            last_ping = now;
        }

        msleep(interval_ms);
    }

    close(s);
    return 0;
}

/* ---------- MAIN (CLIENT ONLY) ---------- */
int main(int argc, char **argv)
{
    const char *mac = NULL;
    uint8_t channel = 1;
    unsigned interval_ms = 150;
    bool verbose = false;

    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "--addr") && i+1 < argc) {
            mac = argv[++i];
        } else if (!strcmp(argv[i], "--channel") && i+1 < argc) {
            channel = (uint8_t)atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--interval-ms") && i+1 < argc) {
            interval_ms = (unsigned)atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--verbose")) {
            verbose = true;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            fprintf(stderr,
                "Usage:\n"
                "  sudo %s --addr AA:BB:CC:DD:EE:FF "
                "[--channel 1] [--interval-ms 150] [--verbose]\n",
                argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            return 1;
        }
    }

    if (!mac) {
        fprintf(stderr, "[ERR] --addr <MAC> wajib.\n");
        return 1;
    }

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    return run_client(mac, channel, interval_ms, verbose);
}

// #include <stdio.h>
// #include <unistd.h>
// #include <sys/socket.h>
// #include <bluetooth/bluetooth.h>
// #include <bluetooth/rfcomm.h>
// #include <string.h>
// #include <time.h>
// #include <fcntl.h>
// #include <errno.h>
// #include <sys/select.h>

// static int connect_with_wait(int s, struct sockaddr_rc *addr)
// {
//     int ret = connect(s, (struct sockaddr *)addr, sizeof(*addr));

//     if (ret == 0) {
//         return 0;  // langsung tersambung
//     }

//     if (errno != EINPROGRESS) {
//         return -1;  // gagal langsung
//     }

//     // Tunggu sampai socket siap (max 5 detik)
//     fd_set wfds;
//     FD_ZERO(&wfds);
//     FD_SET(s, &wfds);

//     struct timeval tv;
//     tv.tv_sec = 5;
//     tv.tv_usec = 0;

//     ret = select(s + 1, NULL, &wfds, NULL, &tv);
//     if (ret <= 0) {
//         return -1;  // timeout atau error
//     }

//     // Periksa apakah connect berhasil
//     int err = 0;
//     socklen_t len = sizeof(err);
//     if (getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
//         return -1;
//     }

//     if (err != 0) {
//         errno = err;
//         return -1;  // connect gagal
//     }

//     return 0; // sukses
// }

// int main() {
//     int s;
//     struct sockaddr_rc addr = {0};
//     char buf[1024];

//     const char *esp32_mac = "40:91:51:B2:84:FA";

// START_CONNECT:

//     s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
//     if (s < 0) {
//         perror("socket");
//         return 1;
//     }

//     // non-blocking
//     fcntl(s, F_SETFL, O_NONBLOCK);

//     addr.rc_family = AF_BLUETOOTH;
//     addr.rc_channel = 1;
//     str2ba(esp32_mac, &addr.rc_bdaddr);

//     printf("Mencoba menghubungkan ke ESP32 (%s)...\n", esp32_mac);

//     while (1) {
//         if (connect_with_wait(s, &addr) == 0) {
//             printf("Terhubung!\n");
//             break;
//         }

//         printf("Gagal. Coba lagi dalam 1 detik...\n");
//         close(s);
//         sleep(1);

//         s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
//         fcntl(s, F_SETFL, O_NONBLOCK);
//     }

//     unsigned long last = 0;

//     while (1) {

//         if (time(NULL) - last >= 1) {
//             const char *msg = "HELLO\n";
//             int w = write(s, msg, strlen(msg));

//             if (w <= 0) {
//                 printf("Koneksi putus. Mencoba reconnect...\n");
//                 close(s);
//                 goto START_CONNECT;
//             }

//             printf("[TX] HELLO\n");
//             last = time(NULL);
//         }

//         int n = read(s, buf, sizeof(buf)-1);
//         if (n > 0) {
//             buf[n] = 0;
//             printf("[RX] %s", buf);
//         } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
//             printf("Read error. Mencoba reconnect...\n");
//             close(s);
//             goto START_CONNECT;
//         }

//         usleep(10000);
//     }

//     close(s);
//     return 0;
// }
