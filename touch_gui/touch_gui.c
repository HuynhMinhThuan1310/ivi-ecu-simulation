#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

/* Định nghĩa màu RGB565 */
#define COLOR_BG      0x08A7  // Xanh đen Cockpit
#define COLOR_BRUSH   0xFFE0  // Vàng rực vẽ nét
#define COLOR_BTN     0xF800  // Đỏ tươi nút bấm
#define COLOR_WHITE   0xFFFF  // Trắng

/* Tọa độ nút bấm CLEAR ở góc trên bên phải */
#define BTN_W 140
#define BTN_H 60
int btn_x1, btn_y1, btn_x2, btn_y2;

void draw_pixel(uint16_t *fbp, int x, int y, int screen_w, uint16_t color) {
    if (x >= 0 && x < screen_w && y >= 0) {
        fbp[y * screen_w + x] = color;
    }
}

/* Vẽ hình chữ nhật đặc */
void draw_rect(uint16_t *fbp, int x1, int y1, int x2, int y2, int screen_w, uint16_t color) {
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            draw_pixel(fbp, x, y, screen_w, color);
        }
    }
}

/* Hàm vẽ nút CLEAR (Nền Đỏ + Viền Trắng + Dấu X) */
void draw_clear_button(uint16_t *fbp, int screen_w, int screen_h) {
    // Vị trí: Cách lề phải 30px, cách lề trên 20px
    btn_x1 = screen_w - BTN_W - 30;
    btn_y1 = 20;
    btn_x2 = btn_x1 + BTN_W;
    btn_y2 = btn_y1 + BTN_H;

    // Vẽ nền nút màu ĐỎ
    draw_rect(fbp, btn_x1, btn_y1, btn_x2, btn_y2, screen_w, COLOR_BTN);

    // Vẽ viền trắng xung quanh nút (dày 2px)
    for (int x = btn_x1; x <= btn_x2; x++) {
        draw_pixel(fbp, x, btn_y1, screen_w, COLOR_WHITE);
        draw_pixel(fbp, x, btn_y1 + 1, screen_w, COLOR_WHITE);
        draw_pixel(fbp, x, btn_y2, screen_w, COLOR_WHITE);
        draw_pixel(fbp, x, btn_y2 - 1, screen_w, COLOR_WHITE);
    }
    for (int y = btn_y1; y <= btn_y2; y++) {
        draw_pixel(fbp, btn_x1, y, screen_w, COLOR_WHITE);
        draw_pixel(fbp, btn_x1 + 1, y, screen_w, COLOR_WHITE);
        draw_pixel(fbp, btn_x2, y, screen_w, COLOR_WHITE);
        draw_pixel(fbp, btn_x2 - 1, y, screen_w, COLOR_WHITE);
    }

    // Vẽ một dấu X to màu trắng ở giữa nút
    int cx = (btn_x1 + btn_x2) / 2;
    int cy = (btn_y1 + btn_y2) / 2;
    for (int d = -16; d <= 16; d++) {
        for (int t = -1; t <= 1; t++) { // Nét dày 3px
            draw_pixel(fbp, cx + d + t, cy + d, screen_w, COLOR_WHITE);
            draw_pixel(fbp, cx + d + t, cy - d, screen_w, COLOR_WHITE);
        }
    }
}

/* Hàm xóa toàn bộ màn hình và vẽ lại nút */
void clear_screen(uint16_t *fbp, int screen_w, int screen_h) {
    for (int i = 0; i < screen_w * screen_h; i++) {
        fbp[i] = COLOR_BG;
    }
    draw_clear_button(fbp, screen_w, screen_h);
}

/* Vẽ nét cọ vẽ ngón tay */
void draw_brush(uint16_t *fbp, int center_x, int center_y, int w, int h, int radius, uint16_t color) {
    for (int y = center_y - radius; y <= center_y + radius; y++) {
        for (int x = center_x - radius; x <= center_x + radius; x++) {
            if (x >= 0 && x < w && y >= 0 && y < h) {
                draw_pixel(fbp, x, y, w, color);
            }
        }
    }
}

int main(void) {
    printf("=== TOUCH GUI - PHIEN BAN CO NUT CLEAR ===\n");

    /* 1. FRAMEBUFFER */
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) { perror("Loi fb0"); return 1; }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

    long screensize = finfo.smem_len; 
    uint16_t *fbp = (uint16_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);

    // Xóa màn hình và vẽ nút CLEAR lần đầu
    clear_screen(fbp, vinfo.xres, vinfo.yres);

    /* 2. CAM UNG */
    int touch_fd = open("/dev/input/event1", O_RDONLY);
    if (touch_fd < 0) { perror("Loi event1"); return 1; }

    struct input_absinfo abs_x, abs_y;
    if (ioctl(touch_fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_x) < 0) ioctl(touch_fd, EVIOCGABS(ABS_X), &abs_x);
    if (ioctl(touch_fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_y) < 0) ioctl(touch_fd, EVIOCGABS(ABS_Y), &abs_y);

    int touch_max_x = abs_x.maximum > 0 ? abs_x.maximum : vinfo.xres;
    int touch_max_y = abs_y.maximum > 0 ? abs_y.maximum : vinfo.yres;

    printf("Nut CLEAR tai: X[%d -> %d], Y[%d -> %d]\n", btn_x1, btn_x2, btn_y1, btn_y2);

    struct input_event ev;
    int raw_x = 0, raw_y = 0;

    while (read(touch_fd, &ev, sizeof(struct input_event)) > 0) {
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) raw_x = ev.value;
            else if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) raw_y = ev.value;
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            if (raw_x > 0 && raw_y > 0) {
                int mapped_x = (long)raw_x * vinfo.xres / touch_max_x;
                int mapped_y = (long)raw_y * vinfo.yres / touch_max_y;

                /* KIEM TRA VA CHAM: Co cham trung nut CLEAR khong? */
                if (mapped_x >= btn_x1 && mapped_x <= btn_x2 &&
                    mapped_y >= btn_y1 && mapped_y <= btn_y2) {
                    
                    printf(">>> DA BAM NUT CLEAR! DANG XOA MAN HINH... <<<\n");
                    clear_screen(fbp, vinfo.xres, vinfo.yres);
                } else {
                    // Neu cham ra ngoai nut: Ve net mau Vang binh thuong
                    draw_brush(fbp, mapped_x, mapped_y, vinfo.xres, vinfo.yres, 6, COLOR_BRUSH);
                }
            }
        }
    }

    munmap(fbp, screensize);
    close(fb_fd);
    close(touch_fd);
    return 0;
}
