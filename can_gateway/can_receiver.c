#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#define CAN_INTERFACE "can0"

static volatile int running = 1;

void sig_handler(int signo) {
    if (signo == SIGINT) running = 0;
}

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void draw_bar(char *buf, int val, int max, int bar_len, const char *color) {
    int filled = (val * bar_len) / max;
    if (filled > bar_len) filled = bar_len;
    if (filled < 0) filled = 0;

    int pos = 0;
    pos += sprintf(buf + pos, "%s[", color);
    for (int i = 0; i < bar_len; i++) {
        if (i < filled) pos += sprintf(buf + pos, "█");
        else           pos += sprintf(buf + pos, "░");
    }
    sprintf(buf + pos, "\033[0m]");
}

int main(void) {
    signal(SIGINT, sig_handler);

    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) { perror("[-] Loi socket CAN"); return 1; }

    struct ifreq ifr;
    strcpy(ifr.ifr_name, CAN_INTERFACE);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("[-] Khong tim thay can0");
        close(s);
        return 1;
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[-] Loi bind socket");
        close(s);
        return 1;
    }

    // 1. CẤU HÌNH TIMEOUT 100ms (CHỐNG TREO APP TUYỆT ĐỐI)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100 ms
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct can_filter rfilter[1];
    rfilter[0].can_id   = 0x100;
    rfilter[0].can_mask = CAN_SFF_MASK;
    setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

    printf("\033[2J\033[?25l");

    struct can_frame frame;
    uint32_t packet_count = 0;
    char bar_speed[128], bar_rpm[128], bar_tps[128];

    uint8_t throttle = 0, speed = 0, gear = 0, seq = 0;
    uint16_t rpm = 0;
    uint64_t last_render_time = 0;
    uint64_t last_packet_time = 0;

    while (running) {
        int nbytes = read(s, &frame, sizeof(struct can_frame));
        uint64_t now = get_time_ms();

        if (nbytes > 0 && frame.can_id == 0x100 && frame.can_dlc == 8) {
            packet_count++;
            last_packet_time = now;
            throttle = frame.data[0];
            speed    = frame.data[1];
            rpm      = (frame.data[2] << 8) | frame.data[3];
            gear     = frame.data[4];
            seq      = frame.data[7];
        }

        // Tần số làm mới giao diện 10 Hz (mỗi 100ms)
        if (now - last_render_time >= 100) {
            last_render_time = now;

            bool is_online = (now - last_packet_time < 1000 && last_packet_time > 0);
            const char *status_str = is_online ? "\033[1;32mONLINE\033[0m" : "\033[1;31mWAITING FOR ESP32...\033[0m";

            const char *gear_str = (gear == 0) ? "\033[1;31m P \033[0m" : 
                                   (gear == 1) ? "\033[1;33m R \033[0m" : 
                                   (gear == 2) ? "\033[1;36m N \033[0m" : 
                                                 "\033[1;32m D \033[0m";

            draw_bar(bar_speed, speed, 200, 25, "\033[1;32m");
            draw_bar(bar_rpm, rpm, 6500, 25, (rpm > 5000) ? "\033[1;31m" : "\033[1;33m");
            draw_bar(bar_tps, throttle, 100, 25, "\033[1;36m");

            printf("\033[H");
            printf("=================================================================\n");
            printf("  \033[1;37m🚗 BEAGLEBONE BLACK - CAN DIGITAL CLUSTER (NON-BLOCKING)\033[0m      \n");
            printf("=================================================================\n");
            printf("  Port: \033[1;32m%s (125 kbps)\033[0m | Bus: %s | Frames: \033[1;36m%u\033[0m | Seq: \033[1;35m#%u\033[0m\n", 
                   CAN_INTERFACE, status_str, packet_count, seq);
            printf("-----------------------------------------------------------------\n\n");
            
            printf("  \033[1;37m[ SPEEDOMETER ]\033[0m    \033[1;32m%3d km/h\033[0m    %s\n\n", 
                   speed, bar_speed);
            printf("  \033[1;37m[ TACHOMETER  ]\033[0m   \033[1;33m%4d RPM\033[0m     %s\n\n", 
                   rpm, bar_rpm);
            printf("  \033[1;37m[ THROTTLE TPS]\033[0m    \033[1;36m%3d %%\033[0m       %s\n\n", 
                   throttle, bar_tps);
            printf("  \033[1;37m[ TRANSMISSION]\033[0m    GEAR: [%s]\n\n", gear_str);

            printf("-----------------------------------------------------------------\n");
            printf("  \033[0;37mCAN Frame: ID=0x%03X DLC=%d | Data: %02X %02X %02X %02X %02X %02X %02X %02X\033[0m\n",
                   frame.can_id, frame.can_dlc,
                   frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                   frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
            printf("  \033[0;33m[Nhan Ctrl+C de thoat ngay lap tuc]\033[0m\n");
            fflush(stdout);
        }
    }

    printf("\033[?25h\n[-] Da dung chuong trinh can_receiver an toan.\n");
    close(s);
    return 0;
}