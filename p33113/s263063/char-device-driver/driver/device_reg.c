#include "device_reg.h"
#include "utils.h"

int register_device(char const * const device_name, struct file_operations const* const fops) {
    int major_number = 0;
    major_number = register_chrdev(major_number, device_name, fops);
    if (major_number < 0) {
        printk(KERN_WARNING "Kek driver: can't register char device with error code %i\n", major_number);
        return major_number;
    }

    printk(KERN_INFO "Kek driver: registered character device with major number %i\n", major_number);
    return major_number;
}

void unregister_device(const int device_major_number, char const * const device_name) {
    if (device_major_number != 0) {
        unregister_chrdev(device_major_number, device_name);
        printk(KERN_INFO "Kek driver: Unregistered device with number %i successfully!\n", device_major_number);
        return;
    }
    printk(KERN_WARNING "Kek driver: Can't unregister zeroed device number!\n");
}