#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#define SCREEN_W 1024
#define SCREEN_H 600
#define CAN_INTERFACE "can0"

/* Bảng màu RGB565 */
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_CYAN      0x07FF
#define COLOR_YELLOW    0xFFE0
#define COLOR_DARK_HUD  0x08A4

static uint16_t map_bg[SCREEN_W * SCREEN_H];
static uint16_t back_buffer[SCREEN_W * SCREEN_H];

/* Trạng thái xe và lộ trình */
static float car_x = SCREEN_W / 2.0f;
static float car_y = SCREEN_H / 2.0f;
static int dest_x = -1;
static int dest_y = -1;

/* Dữ liệu CAN thời gian thực */
static int current_speed = 0;   // km/h
static int current_rpm   = 800; // RPM
static int current_gear  = 0;   // 0=P, 3=D

/* Nút RESET tròn góc trên phải */
#define BTN_R 24
static int btn_cx = SCREEN_W - 45;
static int btn_cy = 40;

static inline void put_pixel(uint16_t *buf, int x, int y, uint16_t color) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
        buf[y * SCREEN_W + x] = color;
    }
}

static void draw_rect(uint16_t *buf, int x1, int y1, int x2, int y2, uint16_t color) {
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            put_pixel(buf, x, y, color);
        }
    }
}

static void draw_circle(uint16_t *buf, int cx, int cy, int r, uint16_t color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                put_pixel(buf, cx + x, cy + y, color);
            }
        }
    }
}

static void draw_thick_line(uint16_t *buf, int x0, int y0, int x1, int y1, int thickness, uint16_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        for (int ty = -thickness / 2; ty <= thickness / 2; ty++) {
            for (int tx = -thickness / 2; tx <= thickness / 2; tx++) {
                put_pixel(buf, x0 + tx, y0 + ty, color);
            }
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ==================================================================== */
/* BỘ DỰNG FONT ĐỒNG HỒ ĐIỆN TỬ 7 ĐOẠN (DIGITAL 7-SEGMENT DISPLAY)      */
/* ==================================================================== */
static void draw_digit_7seg(uint16_t *buf, int x, int y, int digit, int w, int h, int thick, uint16_t color) {
    static const uint8_t segs[10] = {
        0b00111111, // 0
        0b00000110, // 1
        0b01011011, // 2
        0b01001111, // 3
        0b01100110, // 4
        0b01101101, // 5
        0b01111101, // 6
        0b00000111, // 7
        0b01111111, // 8
        0b01101111  // 9
    };
    if (digit < 0 || digit > 9) return;
    uint8_t mask = segs[digit];
    int mid_y = y + h / 2;

    if (mask & 0x01) draw_rect(buf, x, y, x + w, y + thick - 1, color);
    if (mask & 0x02) draw_rect(buf, x + w - thick + 1, y, x + w, mid_y, color);
    if (mask & 0x04) draw_rect(buf, x + w - thick + 1, mid_y, x + w, y + h, color);
    if (mask & 0x08) draw_rect(buf, x, y + h - thick + 1, x + w, y + h, color);
    if (mask & 0x10) draw_rect(buf, x, mid_y, x + thick - 1, y + h, color);
    if (mask & 0x20) draw_rect(buf, x, y, x + thick - 1, mid_y, color);
    if (mask & 0x40) draw_rect(buf, x, mid_y - thick / 2, x + w, mid_y + thick / 2, color);
}

static void draw_number_7seg(uint16_t *buf, int right_x, int y, int num, int w, int h, int thick, uint16_t color) {
    char s[16];
    snprintf(s, sizeof(s), "%d", num);
    int len = strlen(s);
    int start_x = right_x - len * (w + thick + 3);

    for (int i = 0; i < len; i++) {
        draw_digit_7seg(buf, start_x + i * (w + thick + 3), y, s[i] - '0', w, h, thick, color);
    }
}

static void draw_label_kmh(uint16_t *buf, int x, int y, uint16_t color) {
    draw_thick_line(buf, x, y, x, y + 16, 2, color);
    draw_thick_line(buf, x + 8, y, x, y + 8, 2, color);
    draw_thick_line(buf, x, y + 8, x + 8, y + 16, 2, color);

    draw_thick_line(buf, x + 12, y, x + 12, y + 16, 2, color);
    draw_thick_line(buf, x + 12, y, x + 16, y + 8, 2, color);
    draw_thick_line(buf, x + 16, y + 8, x + 20, y, 2, color);
    draw_thick_line(buf, x + 20, y, x + 20, y + 16, 2, color);

    draw_thick_line(buf, x + 23, y + 16, x + 27, y, 2, color);

    draw_thick_line(buf, x + 30, y, x + 30, y + 16, 2, color);
    draw_thick_line(buf, x + 38, y, x + 38, y + 16, 2, color);
    draw_thick_line(buf, x + 30, y + 8, x + 38, y + 8, 2, color);
}

static void draw_label_rpm(uint16_t *buf, int x, int y, uint16_t color) {
    draw_thick_line(buf, x, y, x, y + 14, 2, color);
    draw_thick_line(buf, x, y, x + 7, y, 2, color);
    draw_thick_line(buf, x + 7, y, x + 7, y + 7, 2, color);
    draw_thick_line(buf, x, y + 7, x + 7, y + 7, 2, color);
    draw_thick_line(buf, x + 2, y + 7, x + 7, y + 14, 2, color);

    draw_thick_line(buf, x + 11, y, x + 11, y + 14, 2, color);
    draw_thick_line(buf, x + 11, y, x + 18, y, 2, color);
    draw_thick_line(buf, x + 18, y, x + 18, y + 7, 2, color);
    draw_thick_line(buf, x + 11, y + 7, x + 18, y + 7, 2, color);

    draw_thick_line(buf, x + 22, y, x + 22, y + 14, 2, color);
    draw_thick_line(buf, x + 22, y, x + 26, y + 6, 2, color);
    draw_thick_line(buf, x + 26, y + 6, x + 30, y, 2, color);
    draw_thick_line(buf, x + 30, y, x + 30, y + 14, 2, color);
}

static void draw_car(uint16_t *buf, int cx, int cy) {
    draw_circle(buf, cx, cy, 20, COLOR_WHITE);
    draw_circle(buf, cx, cy, 16, COLOR_RED);
    draw_circle(buf, cx, cy, 8, COLOR_YELLOW);
}

static void draw_reset_button(uint16_t *buf) {
    draw_circle(buf, btn_cx, btn_cy, BTN_R, COLOR_WHITE);
    draw_circle(buf, btn_cx, btn_cy, BTN_R - 3, COLOR_RED);
    draw_thick_line(buf, btn_cx - 12, btn_cy, btn_cx + 12, btn_cy, 3, COLOR_WHITE);
    draw_thick_line(buf, btn_cx, btn_cy - 12, btn_cx, btn_cy + 12, 3, COLOR_WHITE);
}

static void draw_cockpit_hud(uint16_t *buf, int speed, int rpm, int gear) {
    int hud_w = 280, hud_h = 135;
    int x1 = 20, y1 = SCREEN_H - hud_h - 20;
    int x2 = x1 + hud_w, y2 = y1 + hud_h;

    draw_rect(buf, x1, y1, x2, y2, COLOR_DARK_HUD);
    for (int x = x1; x <= x2; x++) { put_pixel(buf, x, y1, COLOR_CYAN); put_pixel(buf, x, y2, COLOR_CYAN); }
    for (int y = y1; y <= y2; y++) { put_pixel(buf, x1, y, COLOR_CYAN); put_pixel(buf, x2, y, COLOR_CYAN); }

    draw_number_7seg(buf, x1 + 130, y1 + 15, speed, 18, 30, 4, COLOR_WHITE);
    draw_label_kmh(buf, x1 + 145, y1 + 25, COLOR_CYAN);

    int speed_bar = (speed * 240) / 200;
    if (speed_bar > 240) speed_bar = 240;
    draw_rect(buf, x1 + 20, y1 + 55, x1 + 20 + speed_bar, y1 + 65, COLOR_GREEN);

    uint16_t rpm_col = (rpm > 4500) ? COLOR_RED : COLOR_YELLOW;
    draw_number_7seg(buf, x1 + 130, y1 + 80, rpm, 12, 20, 3, rpm_col);
    draw_label_rpm(buf, x1 + 145, y1 + 85, COLOR_CYAN);

    int rpm_bar = (rpm * 240) / 6500;
    if (rpm_bar > 240) rpm_bar = 240;
    draw_rect(buf, x1 + 20, y1 + 110, x1 + 20 + rpm_bar, y1 + 118, rpm_col);

    uint16_t gear_col = (gear == 0) ? COLOR_RED : COLOR_GREEN;
    draw_circle(buf, x2 - 30, y1 + 30, 14, gear_col);
    if (gear == 0) {
        draw_thick_line(buf, x2 - 34, y1 + 22, x2 - 34, y1 + 38, 2, COLOR_WHITE);
        draw_thick_line(buf, x2 - 34, y1 + 22, x2 - 27, y1 + 22, 2, COLOR_WHITE);
        draw_thick_line(buf, x2 - 27, y1 + 22, x2 - 27, y1 + 30, 2, COLOR_WHITE);
        draw_thick_line(buf, x2 - 34, y1 + 30, x2 - 27, y1 + 30, 2, COLOR_WHITE);
    } else {
        draw_thick_line(buf, x2 - 34, y1 + 22, x2 - 34, y1 + 38, 2, COLOR_WHITE);
        draw_thick_line(buf, x2 - 34, y1 + 22, x2 - 28, y1 + 24, 2, COLOR_WHITE);
        draw_thick_line(buf, x2 - 28, y1 + 24, x2 - 28, y1 + 36, 2, COLOR_WHITE);
        draw_thick_line(buf, x2 - 34, y1 + 38, x2 - 28, y1 + 36, 2, COLOR_WHITE);
    }
}

static void update_display(uint16_t *fbp) {
    memcpy(back_buffer, map_bg, SCREEN_W * SCREEN_H * sizeof(uint16_t));

    if (dest_x >= 0 && dest_y >= 0) {
        draw_thick_line(back_buffer, (int)car_x, (int)car_y, dest_x, dest_y, 6, COLOR_CYAN);
        draw_thick_line(back_buffer, (int)car_x, (int)car_y, dest_x, dest_y, 2, COLOR_WHITE);
        draw_circle(back_buffer, dest_x, dest_y, 14, COLOR_GREEN);
        draw_circle(back_buffer, dest_x, dest_y, 6, COLOR_RED);
    }

    draw_car(back_buffer, (int)car_x, (int)car_y);
    draw_reset_button(back_buffer);
    draw_cockpit_hud(back_buffer, current_speed, current_rpm, current_gear);

    memcpy(fbp, back_buffer, SCREEN_W * SCREEN_H * sizeof(uint16_t));
}

int main(void) {
    printf("====================================================\n");
    printf("  CAR-IVI NAVIGATION ENGINE WITH DIGITAL SPEED HUD  \n");
    printf("====================================================\n");

    /* 1. Mở file bản đồ */
    FILE *fp = fopen("map_1024x600.raw", "rb");
    if (!fp) fp = fopen("map_1024x768.raw", "rb");
    if (!fp) { perror("[-] Loi mo ban do"); return 1; }
    fread(map_bg, sizeof(uint16_t), SCREEN_W * SCREEN_H, fp);
    fclose(fp);

    /* 2. Mở Framebuffer */
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) { perror("[-] Loi fb0"); return 1; }
    long screensize = SCREEN_W * SCREEN_H * sizeof(uint16_t);
    uint16_t *fbp = (uint16_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);

    /* 3. Mở cảm ứng event1 */
    int touch_fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
    struct input_absinfo abs_x, abs_y;
    int touch_max_x = 2048, touch_max_y = 2048;
    if (touch_fd >= 0) {
        if (ioctl(touch_fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x) >= 0 && abs_x.maximum > 0)
            touch_max_x = abs_x.maximum;
        if (ioctl(touch_fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_y) >= 0 && abs_y.maximum > 0)
            touch_max_y = abs_y.maximum;
    }

    /* 4. Mở SocketCAN can0 */
    int can_fd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW);
    if (can_fd >= 0) {
        struct ifreq ifr;
        strcpy(ifr.ifr_name, CAN_INTERFACE);
        if (ioctl(can_fd, SIOCGIFINDEX, &ifr) >= 0) {
            struct sockaddr_can addr;
            memset(&addr, 0, sizeof(addr));
            addr.can_family = AF_CAN;
            addr.can_ifindex = ifr.ifr_ifindex;
            bind(can_fd, (struct sockaddr *)&addr, sizeof(addr));

            struct can_filter rfilter[1];
            rfilter[0].can_id = 0x100;
            rfilter[0].can_mask = CAN_SFF_MASK;
            setsockopt(can_fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
        }
    }

    update_display(fbp);
    printf("[+] Khoi dong thanh cong! Dong ho toc do da san sang.\n");

    struct input_event ev;
    int raw_x = -1, raw_y = -1;
    struct can_frame cframe;

    while (1) {
        bool need_redraw = false;

        // Đọc dữ liệu CAN Bus
        if (can_fd >= 0) {
            while (read(can_fd, &cframe, sizeof(struct can_frame)) > 0) {
                if (cframe.can_id == 0x100 && cframe.can_dlc == 8) {
                    current_speed = cframe.data[1];
                    current_rpm   = (cframe.data[2] << 8) | cframe.data[3];
                    current_gear  = cframe.data[4];
                    need_redraw = true;
                }
            }
        }

        // Đọc cảm ứng
        if (touch_fd >= 0) {
            while (read(touch_fd, &ev, sizeof(struct input_event)) > 0) {
                if (ev.type == EV_ABS) {
                    if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) raw_x = ev.value;
                    else if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) raw_y = ev.value;
                } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                    if (raw_x >= 0 && raw_y >= 0) {
                        int px = (long)raw_x * SCREEN_W / touch_max_x;
                        int py = (long)raw_y * SCREEN_H / touch_max_y;

                        int dist_sq = (px - btn_cx) * (px - btn_cx) + (py - btn_cy) * (py - btn_cy);
                        if (dist_sq <= BTN_R * BTN_R * 2) {
                            car_x = SCREEN_W / 2.0f;
                            car_y = SCREEN_H / 2.0f;
                            dest_x = -1;
                            dest_y = -1;
                        } else {
                            dest_x = px;
                            dest_y = py;
                        }
                        need_redraw = true;
                    }
                }
            }
        }

        // Mô phỏng xe chạy theo tốc độ CAN
        if (dest_x >= 0 && dest_y >= 0 && current_speed > 0) {
            float dx = dest_x - car_x;
            float dy = dest_y - car_y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist > 3.0f) {
                float step = (current_speed * 0.08f);
                if (step > dist) step = dist;
                car_x += (dx / dist) * step;
                car_y += (dy / dist) * step;
                need_redraw = true;
            } else {
                dest_x = -1;
                dest_y = -1;
                need_redraw = true;
            }
        }

        if (need_redraw) {
            update_display(fbp);
        }

        usleep(25000); // 40 FPS
    }

    munmap(fbp, screensize);
    close(fb_fd);
    if (touch_fd >= 0) close(touch_fd);
    if (can_fd >= 0) close(can_fd);
    return 0;
}
