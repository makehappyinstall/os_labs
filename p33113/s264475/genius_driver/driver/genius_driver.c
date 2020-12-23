#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include "registration_and_io.h"

MODULE_AUTHOR("TohaRhymes");
MODULE_DESCRIPTION("Device that returns calling process id.");
MODULE_LICENSE("GPL");


static struct file_operations device_fops = {
        .owner   = THIS_MODULE,
        .read    = device_file_read,
};

static int device_major_number = 0;
static const char device_name[] = "genius";


static int driver_init(void) {
    device_major_number = register_device(device_name, &device_fops);
    printk(KERN_INFO
    "genius: driver has been initialized \n");
    return 0;
}

static void driver_exit(void) {
    unregister_device(device_major_number, device_name);
    printk(KERN_INFO
    "genius: driver has been destroyed.\n");
}

module_init(driver_init);
module_exit(driver_exit);
