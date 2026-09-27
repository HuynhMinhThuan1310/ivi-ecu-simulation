#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#define DRIVER_NAME "mydev"
#define BUFFER_SIZE 256

// Định nghĩa mã lệnh IOCTL chuẩn Linux
#define MYDEV_MAGIC 'k'
#define MYDEV_SET_VAL _IOW(MYDEV_MAGIC, 1, int)
#define MYDEV_GET_VAL _IOR(MYDEV_MAGIC, 2, int)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thuan");
MODULE_DESCRIPTION("Character Device Driver with IOCTL - Step 1");

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;
static struct device *my_device;

static char kernel_buffer[BUFFER_SIZE];
static size_t data_size = 0;
static int stored_value = 0; // Biến lưu giá trị cài đặt qua IOCTL

static int mydev_open(struct inode *inode, struct file *file) {
    pr_info("mydev: Thiet bi duoc MO (OPEN)\n");
    return 0;
}

static int mydev_release(struct inode *inode, struct file *file) {
    pr_info("mydev: Thiet bi duoc DONG (RELEASE)\n");
    return 0;
}

static ssize_t mydev_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
    size_t bytes_to_read;

    if (*ppos >= data_size) {
        return 0; // Hết dữ liệu (EOF)
    }

    bytes_to_read = min(count, data_size - (size_t)*ppos);

    if (copy_to_user(user_buf, kernel_buffer + *ppos, bytes_to_read)) {
        return -EFAULT;
    }

    *ppos += bytes_to_read;
    pr_info("mydev: Da doc %zu bytes\n", bytes_to_read);
    return bytes_to_read;
}

static ssize_t mydev_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos) {
    size_t bytes_to_write = min(count, (size_t)(BUFFER_SIZE - 1));

    if (copy_from_user(kernel_buffer, user_buf, bytes_to_write)) {
        return -EFAULT;
    }

    kernel_buffer[bytes_to_write] = '\0';
    data_size = bytes_to_write;
    *ppos = 0;

    pr_info("mydev: Da ghi %zu bytes vao kernel: %s\n", bytes_to_write, kernel_buffer);
    return bytes_to_write;
}

static long mydev_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    int temp_val;

    switch (cmd) {
        case MYDEV_SET_VAL:
            if (copy_from_user(&temp_val, (int __user *)arg, sizeof(int))) {
                return -EFAULT;
            }
            stored_value = temp_val;
            pr_info("mydev: [IOCTL] Da dat gia tri moi = %d\n", stored_value);
            break;

        case MYDEV_GET_VAL:
            if (copy_to_user((int __user *)arg, &stored_value, sizeof(int))) {
                return -EFAULT;
            }
            pr_info("mydev: [IOCTL] Da doc gia tri = %d\n", stored_value);
            break;

        default:
            pr_warn("mydev: [IOCTL] Lenh khong hop le: 0x%x\n", cmd);
            return -ENOTTY;
    }

    return 0;
}

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = mydev_open,
    .release        = mydev_release,
    .read           = mydev_read,
    .write          = mydev_write,
    .unlocked_ioctl = mydev_ioctl,
};

static int __init mydev_init(void) {
    int ret;

    ret = alloc_chrdev_region(&dev_num, 0, 1, DRIVER_NAME);
    if (ret < 0) {
        pr_err("mydev: Loi cap phat major/minor\n");
        return ret;
    }

    cdev_init(&my_cdev, &fops);
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("mydev: Loi them cdev\n");
        goto unregister_chrdev;
    }

    my_class = class_create(DRIVER_NAME);
    if (IS_ERR(my_class)) {
        ret = PTR_ERR(my_class);
        pr_err("mydev: Loi tao class\n");
        goto del_cdev;
    }

    my_device = device_create(my_class, NULL, dev_num, NULL, DRIVER_NAME);
    if (IS_ERR(my_device)) {
        ret = PTR_ERR(my_device);
        pr_err("mydev: Loi tao device\n");
        goto destroy_class;
    }

    pr_info("mydev: Khoi tao kem IOCTL thanh cong! Major = %d, Minor = %d\n", MAJOR(dev_num), MINOR(dev_num));
    return 0;

destroy_class:
    class_destroy(my_class);
del_cdev:
    cdev_del(&my_cdev);
unregister_chrdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit mydev_exit(void) {
    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("mydev: Da go bo driver thanh cong!\n");
}

module_init(mydev_init);
module_exit(mydev_exit);
