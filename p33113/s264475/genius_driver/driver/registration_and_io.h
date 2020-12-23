#pragma once

#include <linux/fs.h>

int register_device(char const *device_name, struct file_operations const *fops);

void unregister_device(int device_major_number, char const *device_name);

ssize_t device_file_read(struct file *file, char *user_buffer, size_t count, loff_t *position);
