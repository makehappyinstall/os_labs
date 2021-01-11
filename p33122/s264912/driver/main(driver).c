#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>

#include <linux/utsname.h>

#include <linux/uidgid.h>
#include <linux/cred.h>

int scull_minor = 0;
int scull_major = 0;

const char* anon = "ANON";

#define DATA_SIZE 100
#define PLACEHOLDER "{USER}"
#define DEL ':'
#define EOF '\0'
#define EOL '\n'

struct char_device {
    char data[DATA_SIZE];
} device;

struct cdev *p_cdev;

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/stat.h>

static char *load_file(char* filename, int *input_size) {
    printk(KERN_INFO "im in load");
    struct kstat *stat;
    struct file *fp;
    mm_segment_t fs;
    loff_t pos = 0;
    char *buf;

    printk(KERN_INFO "im starting read file");
    fp = filp_open(filename, O_RDONLY, 0644);
    if (IS_ERR(fp)) {
        printk("Open file error!\n");
        return ERR_PTR(-ENOENT);
    }

    printk(KERN_INFO "Im read file");
    fs = get_fs();
    set_fs(KERNEL_DS);

    printk(KERN_INFO "Hi kmalloc");
    stat =(struct kstat *) kmalloc(sizeof(struct kstat), GFP_KERNEL);
    printk(KERN_INFO "Bye kmalloc");
    if (!stat)
        return ERR_PTR(-ENOMEM);
    printk(KERN_INFO "Wow");
    vfs_stat(filename, stat);
    printk(KERN_INFO "You are very stupid girl");
    *input_size = stat->size;

    printk(KERN_INFO "Hi kmalloc");
    buf = kmalloc(*input_size, GFP_KERNEL);
    if (!buf) {
        kfree(stat);
        printk("malloc input buf error!\n");
        return ERR_PTR(-ENOMEM);
    }
    printk(KERN_INFO "Bye kmalloc");
    kernel_read(fp, buf, *input_size, &pos);
    printk(KERN_INFO "size %d", *input_size);
    filp_close(fp, NULL);
    set_fs(fs);
    kfree(stat);
    return buf;
}

char replace_the_name(char* orig, char* username, size_t orig_len, size_t username_len) {
    char* placeholder = PLACEHOLDER;
    size_t placeholder_len = 6;

    size_t i;

    size_t fst = -1;
    size_t end = -1;
    char have_found = 0;
    for (i = 0; i < orig_len - placeholder_len; i++){
        if (*(orig + i) == EOF) {
            break;
        }
        if (orig[i] == placeholder[0]) {
            fst = i;
            size_t inner_i;
            for(inner_i = 0; inner_i < placeholder_len; i++, inner_i++) {
                if (orig[i] != placeholder[inner_i]) {
                    break;
                } else if (inner_i == placeholder_len - 1) {
                    end = i;
                }
            }
            if (fst != -1 && end != -1) {
                have_found = 1;
                break;
            }
        }
    }

    if (have_found) {
        int diff = username_len - placeholder_len;
        // Shifting after the placeholder
        char tmp[orig_len - end - 1];
        size_t tmp_i;
        size_t ended_at;
        for (i = end + 1, tmp_i = 0; i < orig_len - diff; i++, tmp_i++) {
            if (orig[i] == EOF){
                tmp[tmp_i] = EOF;
                ended_at = i;
                break;
            }
            tmp[tmp_i] = orig[i];
        }

        for (i = end + 1 + diff, tmp_i = 0; i < ended_at+diff; i++, tmp_i++) {
            orig[i] = tmp[tmp_i];
        }
        orig[i] = EOF;

        // Pasting username
        for (i = fst, tmp_i = 0; tmp_i < username_len; i++, tmp_i++){
            orig[i] = username[tmp_i];
        }

        replace_the_name(orig + i, username, orig_len - i, username_len);
        return 1;
    } else {
        return 0;
    }
}

char* get_user_id(int* uid_len) {
    unsigned int cur_uid = get_current_user()->uid.val;

    printk(KERN_INFO "uid digit: %d\n", cur_uid);

    int uid_l = 1;
    int ten = 10;
    while(cur_uid / ten != 0){
        uid_l++;
        ten *= 10;
    }

    printk(KERN_INFO "UID LEN: %d", uid_l);

    char* uid_str = (char*) kmalloc(uid_l+1, GFP_KERNEL);
    ten = 1;
    size_t i;
    for(i = 0; i < uid_l; i++){
        uid_str[uid_l - i - 1] = (char) (cur_uid / ten % 10) + 48;
        ten *= 10;
    }
    uid_str[uid_l] = EOF;
    *uid_len = uid_l;

    return uid_str;
}

char* get_username(char* uid, int uid_len, int* username_len) {
    int input_size;
    printk(KERN_INFO "starting to load");
    char* file_buff = load_file("/etc/passwd", &input_size);
    printk(KERN_INFO "finished loading file");
    printk(KERN_INFO "file %s", file_buff+2300);

    size_t i, inner_i;
    size_t name_str = 0, name_end;
    char found_name = 0;
    int del_count = 2;
    for (i = 0; i < input_size || file_buff[i] != EOF; i++) {
        if (file_buff[i] == DEL) {
            printk(KERN_INFO "Found del char (%c, %lu)", DEL, i);
            del_count -= 1;
            if (del_count == 1) {
                // Remembering username end index
                name_end = i;
            } else if (del_count == 0) {
                // Checking for correct UID
                char is_correct = 1;

                printk(KERN_INFO "Found uid");

                i++;
                for (inner_i = 0; inner_i < uid_len; i++, inner_i++) {
                    printk(KERN_INFO "Comparing with %s: %c %c", uid, uid[inner_i], file_buff[i]);
                    if (uid[inner_i] != file_buff[i]) is_correct = 0;
                }
                if (is_correct == 1) {
                    found_name = 1;
                    break;
                }
            }
        } else if (file_buff[i] == EOL) {
            // Moving carret to the next line
            name_str = i + 1;
            name_end = -1;
            del_count = 2;
        }
    }

    if (found_name) {
        *username_len = name_end - name_str;
        char* res = (char*) kmalloc((*username_len)+1, GFP_KERNEL);

        for (i = name_str, inner_i = 0; i < name_end; i++, inner_i++) {
            res[inner_i] = file_buff[i];
        }
        res[*username_len] = EOF;

        printk(KERN_INFO "scull: resulted username = %s", res);
        return res;
    } else {
        return anon;
    }
}

ssize_t scull_read(struct file *flip, char __user *buf, size_t count, loff_t *f_pos)
{
int rv;

//теперь нужно получить тут юзер нейм, для этого распарсить file_value и найти там то что после юид и двух :
//char* to_replace = "{USER}";
//char* result_str = str_replace(device.data, to_replace, current_user);

// strcpy(result_str, device.data);
int uid_len;
char* cur_uid = get_user_id(&uid_len);

printk(KERN_INFO "uid string %s", cur_uid);

int username_len;
char* username = get_username(cur_uid, uid_len, &username_len);

replace_the_name(device.data, username, DATA_SIZE, username_len);

rv = copy_to_user(buf, device.data, count);
//free(result_str);

return rv;
}

ssize_t scull_write(struct file *flip, const char __user *buf, size_t count, loff_t *f_pos) {
int rv;

printk(KERN_INFO "scull: write to device\n");

rv = copy_from_user(device.data, buf, count);

return rv;
}

int scull_open(struct inode *inode, struct file *flip) {
    printk(KERN_INFO "scull: device is opend\n");

    return 0;
}

int scull_release(struct inode *inode, struct file *flip) {
    printk(KERN_INFO "scull: device is closed\n");

    return 0;
}

struct file_operations scull_fops = {
        .owner = THIS_MODULE,
        .read = scull_read,
        .write = scull_write,
        .open = scull_open,
        .release = scull_release,
};

void scull_cleanup_module(void) {
    dev_t devno = MKDEV(scull_major, scull_minor);

    cdev_del(p_cdev);

    unregister_chrdev_region(devno, 1);
}

static int scull_init_module(void) {
    int rv;
    dev_t dev;

    rv = alloc_chrdev_region(&dev, scull_minor, 1, "scull");

    if (rv) {
        printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
        return rv;
    }

    scull_major = MAJOR(dev);

    p_cdev = cdev_alloc();
    cdev_init(p_cdev, &scull_fops);

    p_cdev->owner = THIS_MODULE;
    p_cdev->ops = &scull_fops;

    rv = cdev_add(p_cdev, dev, 1);

    if (rv)
        printk(KERN_NOTICE "Error %d adding scull", rv);

    printk(KERN_INFO "scull: register device major = %d minor = %d\n", scull_major, scull_minor);

    return 0;
}

MODULE_AUTHOR("Name Surname");
MODULE_LICENSE("GPL");

module_init(scull_init_module);
module_exit(scull_cleanup_module);
