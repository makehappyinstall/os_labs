#pragma once
#include <linux/fs.h>

// Registers new character device with provided name and set syscall callbacks.
// Returns major number of registered device or error code
int register_device(char const * device_name, struct file_operations const * fops);

// Unregisters existing character device with provided major number and name
void unregister_device(int device_major_number, char const * device_name);