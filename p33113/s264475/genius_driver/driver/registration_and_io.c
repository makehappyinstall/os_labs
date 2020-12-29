#include "registration_and_io.h"
#include <linux/uaccess.h>
#include <asm/current.h>


int register_device(char const *const device_name, struct file_operations const *const fops) {
    int major_number = 0;
    major_number = register_chrdev(major_number, device_name, fops);
    if (major_number < 0) {
        printk(KERN_WARNING
        "genius: can't register device. Error %i\n", major_number);
        return major_number;
    }
    printk(KERN_INFO
    "genius: device is registered, major number: %i\n", major_number);
    return major_number;
}

void unregister_device(const int device_major_number, char const *const device_name) {
    if (device_major_number != 0) {
        unregister_chrdev(device_major_number, device_name);
        printk(KERN_INFO
        "genius: device is unregistered, major number: %i!\n", device_major_number);
        return;
    }
    printk(KERN_WARNING
    "genius: can't unregister zeroed device number!\n");
}


ssize_t device_file_read(struct file *file, char *__user user_buffer, size_t count, loff_t *position) {
    char answer_buffer[8] = {};
    ssize_t string_len;

    sprintf(answer_buffer, "%d", current->pid);
    string_len = sizeof(answer_buffer);
    /* If position is behind the end of a file we have nothing to read */
    /* If a user tries to read more than we have, read only as many bytes as we have */
    if (*position >= string_len)
        return 0;
    if (*position + count > string_len)
        count = string_len - *position;

    if (_copy_to_user(user_buffer, answer_buffer + *position, count) != 0)
        return -EFAULT;

    /* Move reading position */
    *position += count;
    return count;
}
