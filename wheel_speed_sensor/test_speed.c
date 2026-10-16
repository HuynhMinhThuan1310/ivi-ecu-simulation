#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>

#define DEVICE_PATH "/dev/speed_dev"

int main() {
    int fd;
    struct pollfd pfd;
    char buffer[32];
    int ret;
    int pulses = 0;

    printf("=========================================\n");
    printf("   CAR-IVI WHEEL SPEEDOMETER TEST        \n");
    printf("=========================================\n");

    // 1. Mở file thiết bị
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Loi: Khong the mo /dev/speed_dev");
        return 1;
    }

    // 2. Cấu hình poll: Theo dõi sự kiện có dữ liệu để đọc (POLLIN)
    pfd.fd = fd;
    pfd.events = POLLIN;

    printf("[USER SPACE] Dang o che do NGU YEN cho banh xe quay (0%% CPU)...\n\n");

    while (1) {
        // HÀM POLL: Ứng dụng rơi vào giấc ngủ sâu tại đây.
        // Chỉ khi có ngắt phần cứng, Kernel mới đánh thức dậy!
        ret = poll(&pfd, 1, -1); // -1: Chờ vô thời hạn cho đến khi có sự kiện

        if (ret < 0) {
            perror("Loi poll");
            break;
        }

        // Khi thức giấc vì có xung mới
        if (pfd.revents & POLLIN) {
            // Đọc số xung từ Driver
            int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                pulses = atoi(buffer);

                // Giả lập tính tốc độ xe: mỗi xung = bánh xe quay 1 vòng
                printf("🚗 [BANH XE QUAY] Xung thu: %3d | Toc do mo phong: %5.1f km/h\n", 
                       pulses, pulses * 1.5);
                fflush(stdout);
            }
        }
    }

    close(fd);
    return 0;
}
