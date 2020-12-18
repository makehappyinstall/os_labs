#include "device_callbacks.h"
#include <linux/uaccess.h>
#include "utils.h"

static unsigned long invokation_count = 0;
static char str_invocation_count_buffer[50] = {};
static const ssize_t string_len = sizeof(str_invocation_count_buffer);

ssize_t kek_file_read(struct file* file, char* __user user_buffer, size_t count, loff_t* position) {
    invokation_count += 1;
    snprintf(str_invocation_count_buffer, 50, "%lu", invokation_count);
    if (*position >= string_len) return 0;
    if (*position + count > string_len) count = string_len - *position;
    if (_copy_to_user(user_buffer, str_invocation_count_buffer + *position, count) != 0) return -EFAULT;
    *position += count;
    return count;
}

