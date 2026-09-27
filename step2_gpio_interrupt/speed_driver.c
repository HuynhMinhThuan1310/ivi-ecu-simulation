#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/poll.h>

#define DRIVER_NAME "speed_dev"
#define GPIO_SPEED_PIN 540        // Chân P9_12 chuẩn trên Kernel 6.6 của BeagleBone Black (4804c000.gpio offset 28)
#define DEBOUNCE_TIME_MS 100      // Lọc chống rung cơ học 100ms

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thuan");
MODULE_DESCRIPTION("Speed Sensor GPIO Interrupt Driver - Step 2 (Final)");

/* Quản lý thiết bị ký tự */
static dev_t dev_num;
static struct cdev speed_cdev;
static struct class *speed_class;
static struct device *speed_device;

/* Quản lý ngắt và dữ liệu xung */
static int irq_number;
static atomic_t pulse_count = ATOMIC_INIT(0);
static ktime_t last_interrupt_time;

/* Quản lý ru ngủ và theo dõi đọc dữ liệu */
static DECLARE_WAIT_QUEUE_HEAD(speed_waitqueue);
static int last_read_pulses = 0; // Ghi nhớ mốc xung mà User Space đã đọc

/* -------------------------------------------------------------
 * 1. HÀM XỬ LÝ NGẮT PHẦN CỨNG (ISR)
 * ------------------------------------------------------------- */
static irqreturn_t speed_irq_handler(int irq, void *dev_id) {
    ktime_t now = ktime_get();
    s64 delta_ms = ktime_to_ms(ktime_sub(now, last_interrupt_time));

    // Lọc nhiễu / Debounce
    if (delta_ms < DEBOUNCE_TIME_MS) {
        return IRQ_HANDLED;
    }
    last_interrupt_time = now;

    // Tăng số xung an toàn
    atomic_inc(&pulse_count);

    // Đánh thức ứng dụng User Space đang ngủ chờ dậy
    wake_up_interruptible(&speed_waitqueue);

    pr_info("speed_driver: [NGAT HOP LE] Da nhan 1 xung! Tong so xung = %d\n", 
            atomic_read(&pulse_count));

    return IRQ_HANDLED;
}

/* -------------------------------------------------------------
 * 2. HÀM ĐỌC DỮ LIỆU (READ)
 * ------------------------------------------------------------- */
static ssize_t speed_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
    char buf[32];
    int len;
    int current_pulses;

    // 1. Đọc số xung hiện tại
    current_pulses = atomic_read(&pulse_count);

    // 2. Cập nhật mốc: Đã đọc tới xung này rồi!
    last_read_pulses = current_pulses;

    // 3. Đổi số thành chuỗi ký tự text
    len = snprintf(buf, sizeof(buf), "%d\n", current_pulses);

    // 4. Gửi sang User Space
    if (copy_to_user(user_buf, buf, len)) {
        return -EFAULT;
    }

    return len;
}

/* -------------------------------------------------------------
 * 3. HÀM POLL (Chỉ kiểm tra trạng thái, KHÔNG sửa dữ liệu)
 * ------------------------------------------------------------- */
static __poll_t speed_poll(struct file *file, poll_table *wait) {
    __poll_t mask = 0;

    // Đăng ký phòng chờ ru ngủ
    poll_wait(file, &speed_waitqueue, wait);

    // Nếu số xung hiện tại KHÁC với số xung đã đọc lần trước -> Có dữ liệu mới!
    if (atomic_read(&pulse_count) != last_read_pulses) {
        mask |= (EPOLLIN | EPOLLRDNORM);
    }

    return mask;
}

/* Bảng liên kết thao tác File */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = speed_read,
    .poll  = speed_poll,
};

/* -------------------------------------------------------------
 * 4. HÀM KHỞI TẠO (insmod)
 * ------------------------------------------------------------- */
static int __init speed_init(void) {
    int ret;

    // A. Cấp phát Character Device
    ret = alloc_chrdev_region(&dev_num, 0, 1, DRIVER_NAME);
    if (ret < 0) return ret;

    cdev_init(&speed_cdev, &fops);
    ret = cdev_add(&speed_cdev, dev_num, 1);
    if (ret < 0) goto unregister_chrdev;

    speed_class = class_create(DRIVER_NAME);
    if (IS_ERR(speed_class)) {
        ret = PTR_ERR(speed_class);
        goto del_cdev;
    }

    speed_device = device_create(speed_class, NULL, dev_num, NULL, DRIVER_NAME);
    if (IS_ERR(speed_device)) {
        ret = PTR_ERR(speed_device);
        goto destroy_class;
    }

    // B. Cấu hình GPIO 540 (P9_12) và Đăng ký Ngắt
    ret = gpio_request(GPIO_SPEED_PIN, "speed_sensor_pin");
    if (ret) {
        pr_err("speed_driver: Khong the yeu cau GPIO %d\n", GPIO_SPEED_PIN);
        goto destroy_device;
    }

    gpio_direction_input(GPIO_SPEED_PIN);

    irq_number = gpio_to_irq(GPIO_SPEED_PIN);
    pr_info("speed_driver: GPIO %d anh xa sang IRQ so: %d\n", GPIO_SPEED_PIN, irq_number);

    ret = request_irq(irq_number, speed_irq_handler, IRQF_TRIGGER_FALLING, "speed_irq", NULL);
    if (ret) {
        pr_err("speed_driver: Khong the dang ky ngat IRQ %d\n", irq_number);
        goto free_gpio;
    }

    last_interrupt_time = ktime_get();
    last_read_pulses = 0;

    pr_info("speed_driver: Khoi tao thanh cong! San sang bat xung tren P9_12.\n");
    return 0;

free_gpio:
    gpio_free(GPIO_SPEED_PIN);
destroy_device:
    device_destroy(speed_class, dev_num);
destroy_class:
    class_destroy(speed_class);
del_cdev:
    cdev_del(&speed_cdev);
unregister_chrdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

/* -------------------------------------------------------------
 * 5. HÀM DỌN DẸP (rmmod)
 * ------------------------------------------------------------- */
static void __exit speed_exit(void) {
    free_irq(irq_number, NULL);
    gpio_free(GPIO_SPEED_PIN);
    device_destroy(speed_class, dev_num);
    class_destroy(speed_class);
    cdev_del(&speed_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("speed_driver: Da go bo driver va tra lai chan GPIO!\n");
}

module_init(speed_init);
module_exit(speed_exit);
