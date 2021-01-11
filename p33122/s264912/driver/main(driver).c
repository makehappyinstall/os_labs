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

struct char_device {
	char data[100];
} device;

struct cdev *p_cdev;



#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/stat.h>

static char *load_file(char* filename, int *input_size)
{
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
        printk(KERN_INFO "size %d", input_size);
        filp_close(fp, NULL);
        set_fs(fs);
        kfree(stat);
        return buf;
}

char replace_the_name(char* orig, char* username, size_t orig_len, size_t username_len) {
	char* placeholder = "{USER}";
	size_t placeholder_len = 6;

	size_t i;

	size_t fst = -1;
	size_t end = -1;
	char have_found = 0;
	for (i = 0; i < orig_len - placeholder_len; i++){
		if (*(orig + i) == '\0') {
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
			if (orig[i] == '\0'){
				tmp[tmp_i] = '\0';
				ended_at = i;
				break;
			}
			tmp[tmp_i] = orig[i];
		}

		for (i = end + 1 + diff, tmp_i = 0; i < ended_at+diff; i++, tmp_i++) {
			orig[i] = tmp[tmp_i];
		}
		orig[i] = '\0';

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

char* split(char* origin, char del){
	size_t i = 0;
	printk(KERN_INFO "not kruto");
	while(origin[i] != '\0'){
		printk(KERN_INFO "before kruto");
		if (origin[i] == del){
			//printk(KERN_INFO origin[i]);
			origin[i++] = '\0';
		}
		i++;
	}
	return (char *) origin;
}

ssize_t scull_read(struct file *flip, char __user *buf, size_t count,
				loff_t *f_pos)
{
	int rv;
	
	int input_size;// ????
	
	printk(KERN_INFO "im starting load");
	char* file_buff = load_file("/etc/passwd", &input_size);
	printk(KERN_INFO "im finished loading file");
	
	//struct file * user_file = filp_open("/etc/passwd", O_RDONLY, 0);
	//char file_value[100000];
	//vfs_read(user_file, file_value, sizeof(file_value), user_file->f_pos);
	
	printk(KERN_INFO "file %s", file_buff+2300);
	
	char origin[] = "fghhj:hjlk:hjjk";
	char del = ':';
	size_t i = 0;
	printk(KERN_INFO "not kruto");
	while(origin[i] != '\0'){
		printk(KERN_INFO "before kruto");
		if (origin[i] == del){
			printk(KERN_INFO "kruto %c", origin[i]);
			origin[i++] = '\0';
		}
		i++;
	}
	
	
	printk(KERN_INFO "scull: read from device\n");
	
	//int uid_l = 1;
	//while(current_user / (10 * uid_l) != 0){
	//	uid_l++;
	//}
	//char uid_str[uid_l];
	//size_t i;
	//for(i = 0; i < uid_l; i++){
	//	uid_str[uid_l - i - 1] = (char) (current_user / (i == 0 ? 1 : 10 * i) % 10) + 48;
	//}
	
	//char* to_replace = "{USER}";
	//char* result_str = str_replace(device.data, to_replace, current_user);

	// strcpy(result_str, device.data);
	
	unsigned int cur_uid = get_current_user()->uid.val; 
	
	printk(KERN_INFO "scull: current user = %d", cur_uid);
	
	char test_user[7] = "TAISIYA";
	
	replace_the_name(device.data, test_user, 100, 7);
	
	rv = copy_to_user(buf, device.data, count);
	//free(result_str);
	
	return rv;
}

ssize_t scull_write(struct file *flip, const char __user *buf, size_t count,
				loff_t *f_pos)
{
	int rv;

	printk(KERN_INFO "scull: write to device\n");

	rv = copy_from_user(device.data, buf, count);

	return rv;
}

int scull_open(struct inode *inode, struct file *flip)
{
	printk(KERN_INFO "scull: device is opend\n");

	return 0;
}

int scull_release(struct inode *inode, struct file *flip)
{
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

void scull_cleanup_module(void)
{
	dev_t devno = MKDEV(scull_major, scull_minor);

	cdev_del(p_cdev);

	unregister_chrdev_region(devno, 1); 
}

static int scull_init_module(void)
{
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
