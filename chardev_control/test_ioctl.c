#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE_PATH "/dev/mydev"

#define MYDEV_MAGIC 'k'
#define MYDEV_SET_VAL _IOW(MYDEV_MAGIC, 1, int)
#define MYDEV_GET_VAL _IOR(MYDEV_MAGIC, 2, int)

int main() {
    int fd;
    int send_val = 2026;
    int recv_val = 0;

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Loi: Khong the mo thiet bi /dev/mydev");
        return 1;
    }

    printf("[USER] Gửi lệnh IOCTL SET gia tri = %d vao Kernel...", send_val);
    if (ioctl(fd, MYDEV_SET_VAL, &send_val) < 0) {
        perror("Loi ioctl SET");
        close(fd);
        return 1;
    }
    printf(" XONG!\n");

    printf("[USER] Gửi lệnh IOCTL GET gia tri tu Kernel ve...");
    if (ioctl(fd, MYDEV_GET_VAL, &recv_val) < 0) {
        perror("Loi ioctl GET");
        close(fd);
        return 1;
    }
    printf(" XONG!\n");

    printf("\n[USER] Ket qua nhan duoc: %d\n", recv_val);
    if (recv_val == send_val) {
        printf("🎉 TEST IOCTL THANH CONG 100%!\n");
    } else {
        printf("❌ TEST THAT BAI: Gia tri khong khop!\n");
    }

    close(fd);
    return 0;
}
