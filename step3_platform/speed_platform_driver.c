#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/poll.h>

/* THƯ VIỆN CHUYÊN DỤNG CHO PLATFORM DRIVER & DEVICE TREE */
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>

#define DRIVER_NAME "speed_dev"
#define DEBOUNCE_TIME_MS 100

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thuan");
MODULE_DESCRIPTION("Automotive Speed Sensor Platform Driver - Step 3 (Final)");

/* Quản lý thiết bị ký tự */
static dev_t dev_num;
static struct cdev speed_cdev;
static struct class *speed_class;
static struct device *speed_device;

/* Quản lý ngắt và dữ liệu */
static struct gpio_desc *speed_gpiod;
static int irq_number;
static atomic_t pulse_count = ATOMIC_INIT(0);
static ktime_t last_interrupt_time;
static DECLARE_WAIT_QUEUE_HEAD(speed_waitqueue);
static int last_read_pulses = 0;

/* -------------------------------------------------------------
 * 1. HÀM XỬ LÝ NGẮT PHẦN CỨNG (ISR)
 * ------------------------------------------------------------- */
static irqreturn_t speed_irq_handler(int irq, void *dev_id) {
    ktime_t now = ktime_get();
    s64 delta_ms = ktime_to_ms(ktime_sub(now, last_interrupt_time));

    if (delta_ms < DEBOUNCE_TIME_MS) {
        return IRQ_HANDLED;
    }
    last_interrupt_time = now;

    atomic_inc(&pulse_count);
    wake_up_interruptible(&speed_waitqueue);

    pr_info("speed_platform: [NGAT HOP LE] Nhan 1 xung! Tong so xung = %d\n", 
            atomic_read(&pulse_count));

    return IRQ_HANDLED;
}

/* -------------------------------------------------------------
 * 2. HÀM READ
 * ------------------------------------------------------------- */
static ssize_t speed_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
    char buf[32];
    int len;
    int current_pulses;

    current_pulses = atomic_read(&pulse_count);
    last_read_pulses = current_pulses;

    len = snprintf(buf, sizeof(buf), "%d\n", current_pulses);
    if (copy_to_user(user_buf, buf, len)) {
        return -EFAULT;
    }
    return len;
}

/* -------------------------------------------------------------
 * 3. HÀM POLL
 * ------------------------------------------------------------- */
static __poll_t speed_poll(struct file *file, poll_table *wait) {
    __poll_t mask = 0;
    poll_wait(file, &speed_waitqueue, wait);

    if (atomic_read(&pulse_count) != last_read_pulses) {
        mask |= (EPOLLIN | EPOLLRDNORM);
    }
    return mask;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = speed_read,
    .poll  = speed_poll,
};

/* -------------------------------------------------------------
 * 4. HÀM PROBE: Chạy tự động khi Kernel tìm thấy phần cứng khớp chuỗi "car-ivi,speed-sensor"
 * ------------------------------------------------------------- */
static int speed_probe(struct platform_device *pdev) {
    struct device *dev = &pdev->dev;
    int ret;

    dev_info(dev, "speed_platform: Bat tay phan cung thanh cong! Bat dau khoi tao...\n");

    // A. Cấp phát file thiết bị /dev/speed_dev
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

    // B. Lấy chân GPIO tự động từ Device Tree (tìm thuộc tính speed-gpios)
    speed_gpiod = gpiod_get(dev, "speed", GPIOD_IN);
    if (IS_ERR(speed_gpiod)) {
        ret = PTR_ERR(speed_gpiod);
        dev_err(dev, "Khong the lay chan GPIO tu Device Tree! (loi: %d)\n", ret);
        goto destroy_device;
    }

    // C. Tự động chuyển chân GPIO sang số hiệu ngắt IRQ
    irq_number = gpiod_to_irq(speed_gpiod);
    dev_info(dev, "Kernel tu cap phat IRQ so: %d cho cam bien toc do\n", irq_number);

    // D. Đăng ký hàm ngắt
    ret = request_irq(irq_number, speed_irq_handler, IRQF_TRIGGER_FALLING, "speed_irq", NULL);
    if (ret) {
        dev_err(dev, "Khong the dang ky ngat IRQ %d\n", irq_number);
        goto put_gpio;
    }

    last_interrupt_time = ktime_get();
    last_read_pulses = 0;

    dev_info(dev, "Platform Driver da san sang bat xung!\n");
    return 0;

put_gpio:
    gpiod_put(speed_gpiod);
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
 * 5. HÀM REMOVE: Chạy tự động khi tháo thiết bị hoặc gỡ driver
 * ------------------------------------------------------------- */
static int speed_remove(struct platform_device *pdev) {
    free_irq(irq_number, NULL);
    gpiod_put(speed_gpiod);

    device_destroy(speed_class, dev_num);
    class_destroy(speed_class);
    cdev_del(&speed_cdev);
    unregister_chrdev_region(dev_num, 1);

    dev_info(&pdev->dev, "Platform Driver da duoc go bo an toan!\n");
    return 0;
}

/* -------------------------------------------------------------
 * 6. BẢNG MAI MỐI DEVICE TREE (OF Match Table)
 * ------------------------------------------------------------- */
static const struct of_device_id speed_of_match[] = {
    { .compatible = "car-ivi,speed-sensor", },
    { /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, speed_of_match);

/* -------------------------------------------------------------
 * 7. KHAI BÁO CẤU TRÚC PLATFORM DRIVER
 * ------------------------------------------------------------- */
static struct platform_driver speed_platform_driver = {
    .probe  = speed_probe,
    .remove = speed_remove,
    .driver = {
        .name           = "car_speed_sensor",
        .of_match_table = speed_of_match,
        .owner          = THIS_MODULE,
    },
};

module_platform_driver(speed_platform_driver);
