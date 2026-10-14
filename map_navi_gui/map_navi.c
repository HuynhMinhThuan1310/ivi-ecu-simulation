#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define SCREEN_W 1024
#define SCREEN_H 600

/* Bảng màu RGB565 */
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_CYAN      0x07FF
#define COLOR_YELLOW    0xFFE0
#define COLOR_DARK_HUD  0x10A2

/* 2 bộ đệm: 1 đệm bản đồ gốc, 1 đệm vẽ đồ họa (Back Buffer) */
static uint16_t map_bg[SCREEN_W * SCREEN_H];
static uint16_t back_buffer[SCREEN_W * SCREEN_H];

/* Tọa độ xe và trạng thái chạm */
static int car_x = SCREEN_W / 2;
static int car_y = SCREEN_H / 2;
static bool need_redraw = true;

/* Nút RESET tròn nhỏ gọn góc trên bên phải */
#define BTN_R 24
static int btn_cx = SCREEN_W - 45;
static int btn_cy = 40;

static inline void put_pixel(uint16_t *buf, int x, int y, uint16_t color) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
        buf[y * SCREEN_W + x] = color;
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

/* Vẽ biểu tượng xe ô tô dẫn đường */
static void draw_car(uint16_t *buf, int cx, int cy) {
    draw_circle(buf, cx, cy, 20, COLOR_WHITE);
    draw_circle(buf, cx, cy, 16, COLOR_RED);
    draw_circle(buf, cx, cy, 8, COLOR_YELLOW);
}

/* Vẽ nút tròn RECENTER góc phải */
static void draw_reset_button(uint16_t *buf) {
    draw_circle(buf, btn_cx, btn_cy, BTN_R, COLOR_WHITE);
    draw_circle(buf, btn_cx, btn_cy, BTN_R - 3, COLOR_RED);
    draw_thick_line(buf, btn_cx - 12, btn_cy, btn_cx + 12, btn_cy, 3, COLOR_WHITE);
    draw_thick_line(buf, btn_cx, btn_cy - 12, btn_cx, btn_cy + 12, 3, COLOR_WHITE);
}

/* Hàm dựng hình hoàn chỉnh trên Back Buffer rồi đẩy 1 lần ra Framebuffer */
static void update_display(uint16_t *fbp) {
    // 1. Phục hồi bản đồ sạch vào RAM đệm
    memcpy(back_buffer, map_bg, SCREEN_W * SCREEN_H * sizeof(uint16_t));

    // 2. Vẽ xe và nút bấm
    draw_car(back_buffer, car_x, car_y);
    draw_reset_button(back_buffer);

    // 3. Đẩy nguyên khung hình hoàn hảo ra màn hình LCD trong 1 lần duy nhất
    memcpy(fbp, back_buffer, SCREEN_W * SCREEN_H * sizeof(uint16_t));
}

int main(void) {
    printf("=== CAR-IVI ANTI-TEARING MAP ENGINE (1024x600) ===\n");

    /* 1. Mở file bản đồ */
    FILE *fp = fopen("map_1024x600.raw", "rb");
    if (!fp) fp = fopen("map_1024x768.raw", "rb");
    if (!fp) {
        perror("[-] Khong the mo file ban do .raw");
        return 1;
    }
    fread(map_bg, sizeof(uint16_t), SCREEN_W * SCREEN_H, fp);
    fclose(fp);

    /* 2. Mở Framebuffer */
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) { perror("[-] Loi fb0"); return 1; }

    long screensize = SCREEN_W * SCREEN_H * sizeof(uint16_t);
    uint16_t *fbp = (uint16_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);

    /* 3. Mở cảm ứng event1 ở chế độ Non-Blocking */
    int touch_fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
    if (touch_fd < 0) {
        perror("[-] Loi mo /dev/input/event1");
        return 1;
    }

    struct input_absinfo abs_x, abs_y;
    int touch_max_x = 2048, touch_max_y = 2048;
    if (ioctl(touch_fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x) >= 0 && abs_x.maximum > 0)
        touch_max_x = abs_x.maximum;
    if (ioctl(touch_fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_y) >= 0 && abs_y.maximum > 0)
        touch_max_y = abs_y.maximum;

    printf("[+] He thong san sang! Touch Max: %dx%d\n", touch_max_x, touch_max_y);
    update_display(fbp);

    struct input_event ev;
    int raw_x = -1, raw_y = -1;

    /* Vòng lặp tối ưu: Giới hạn 40 FPS (chu kỳ 25ms) loại bỏ hoàn toàn hiện tượng xé hình */
    while (1) {
        while (read(touch_fd, &ev, sizeof(struct input_event)) > 0) {
            if (ev.type == EV_ABS) {
                if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) raw_x = ev.value;
                else if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) raw_y = ev.value;
            } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                if (raw_x >= 0 && raw_y >= 0) {
                    int px = (long)raw_x * SCREEN_W / touch_max_x;
                    int py = (long)raw_y * SCREEN_H / touch_max_y;

                    // Kiểm tra chạm trúng nút tròn RESET
                    int dist_sq = (px - btn_cx) * (px - btn_cx) + (py - btn_cy) * (py - btn_cy);
                    if (dist_sq <= BTN_R * BTN_R * 2) {
                        car_x = SCREEN_W / 2;
                        car_y = SCREEN_H / 2;
                        need_redraw = true;
                    } else {
                        // Kéo xe bám theo ngón tay
                        car_x = px;
                        car_y = py;
                        need_redraw = true;
                    }
                }
            }
        }

        if (need_redraw) {
            update_display(fbp);
            need_redraw = false;
        }

        usleep(25000);
    }

    munmap(fbp, screensize);
    close(fb_fd);
    close(touch_fd);
    return 0;
}
