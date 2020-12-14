#pragma once
#include <linux/fs.h>

ssize_t kek_file_read(struct file* file, char* user_buffer, size_t count, loff_t* position);
