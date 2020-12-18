#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include "device_reg.h"
#include "device_callbacks.h"
#include "utils.h"

MODULE_AUTHOR("rbetik12");

MODULE_LICENSE("GPL");

MODULE_DESCRIPTION("Kek driver for ITMO OS course. "
                   "Simple char device that returns number of read attempts from it on read syscall invocation");


static int device_major_number = 0;
static const char device_name[] = "kek";

static struct file_operations device_fops = {
        .owner   = THIS_MODULE,
        .read    = kek_file_read,
};

static int driver_init(void) {
    device_major_number = register_device(device_name, &device_fops);
    printk(KERN_INFO "Kek driver: Initialized driver successfully!\n");
    return 0;
}

static void driver_exit(void) {
    unregister_device(device_major_number, device_name);
    printk(KERN_INFO "Kek driver: Destroyed driver successfully!\n");
}

module_init(driver_init);
module_exit(driver_exit);