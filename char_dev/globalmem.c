#include<linux/module.h>
#include<linux/fs.h>
#include<linux/init.h>
#include<linux/cdev.h>
#include<linux/slab.h>
#include<linux/uaccess.h>
#include<linux/atomic.h>
#include<linux/mutex.h>
#include<linux/wait.h>
#include<linux/sched.h>
#include<linux/poll.h>
#include<linux/fs.h>

#define GLOBALMEM_SIZE 0X1000
#define MEM_CLEAR 0X01
#define GLOBALMEM_MAJOR 230
#define DEVICE_NUM 10

static int globalmem_major = GLOBALMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO);

struct globalmem_dev{
	struct cdev cdev;
	unsigned char mem[GLOBALMEM_SIZE];
	unsigned int current_len;
	struct mutex mem_lock;
	wait_queue_head_t r_wait;
	wait_queue_head_t w_wait;
	struct fasync_struct *async_queue;
};

struct globalmem_dev *globalmem_devp;

static int globalmem_open(struct inode *inode, struct file *filp)
{
	struct globalmem_dev *dev = container_of(inode->i_cdev, struct globalmem_dev, cdev);
	filp->private_data = dev;
	return 0;
}

static int globalmem_fasync(int fd, struct file *filp, int mode)
{
	struct globalmem_dev *dev = filp->private_data;
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int globalmem_release(struct inode *inode, struct file *filp)
{
	globalmem_fasync(-1, filp, 0);
	filp->private_data = NULL;
	return 0;
}

static long globalmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct globalmem_dev *dev = filp->private_data;
	mutex_lock(&((struct globalmem_dev *)filp->private_data)->mem_lock);
	switch (cmd) {
	case MEM_CLEAR:
		memset(dev->mem, 0, GLOBALMEM_SIZE);
		printk(KERN_INFO "globalmem is set to zero");
		break;
	default:
		mutex_unlock(&dev->mem_lock);
		return -EINVAL;
	}
	mutex_unlock(&dev->mem_lock);
	return 0;
}

static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	//unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct globalmem_dev *dev = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);
	mutex_lock(&dev->mem_lock);
	add_wait_queue(&dev->r_wait, &wait);
	while (dev->current_len == 0) {
		if (filp->f_flags & O_NONBLOCK) {
			printk(KERN_INFO "no msg to read, and no block\n ");
			ret = -EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&dev->mem_lock);
		schedule();

		if (signal_pending(current)) {
			printk(KERN_INFO "signal to wake up the read block\n ");
			ret = -ERESTARTSYS;
			goto out2;
		}
		mutex_lock(&dev->mem_lock);
	}

	//if (p >= GLOBALMEM_SIZE) {
	//	return 0;
	//}

	if (count > dev->current_len) {
		count = dev->current_len;
	}

	if (copy_to_user(buf, dev->mem, count)) {
		ret = -EFAULT;
		goto out;
	} else {
		//*ppos += count;
		ret = count;
		memcpy(dev->mem, dev->mem + count, dev->current_len - count);
		dev->current_len -= count;
		printk(KERN_INFO "read %u current_len = %lu\n", count, dev->current_len);
		wake_up_interruptible(&dev->w_wait);
	}
out:
	mutex_unlock(&dev->mem_lock);
out2:
	remove_wait_queue(&dev->r_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static ssize_t globalmem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
	//unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	DECLARE_WAITQUEUE(wait, current);
	struct globalmem_dev *dev = filp->private_data;
	mutex_lock(&dev->mem_lock);
	add_wait_queue(&dev->w_wait, &wait);

	while (dev->current_len == GLOBALMEM_SIZE) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			printk(KERN_INFO "no memory to write, and no block\n ");
			goto out;
		}

		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&dev->mem_lock);
		schedule();

		if (signal_pending(current)) {
			printk(KERN_INFO "signal to wake up write block\n");
			ret = -ERESTARTSYS;
			goto out2;
		}
		mutex_lock(&dev->mem_lock);
	}

	//if (p > GLOBALMEM_SIZE) {
	//	return 0;
	//}

	if (count > GLOBALMEM_SIZE - dev->current_len) {
		count = GLOBALMEM_SIZE - dev->current_len;
	}

	//if (count > GLOBALMEM_SIZE - p) {
	//	count = GLOBALMEM_SIZE - p;
	//}

	//mutex_lock(&dev->mem_lock);
	if (copy_from_user(dev->mem + dev->current_len, buf, count)) {
	  ret = -EFAULT;
	  goto out;
	}
	else {
		//*ppos += count;
		dev->current_len += count;
		ret = count;
		printk(KERN_INFO "written %u current_len =  %lu\n", count, dev->current_len);
		wake_up_interruptible(&dev->r_wait);

		//kill fasync
		if (dev->async_queue) {
			kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
			printk(KERN_DEBUG "%s kill SIGIO\n", __func__);
		}
	}
out:
	mutex_unlock(&dev->mem_lock);
out2:
	remove_wait_queue(&dev->w_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

/*static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret = 0;
	switch (orig) {
	case 0:
		if (offset < 0 || (unsigned int)offset > GLOBALMEM_SIZE) {
			ret = -EINVAL;
			break;
		}
		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;
	case 1:
		if ((filp->f_pos + offset) > GLOBALMEM_SIZE || (filp->f_pos + offset) < 0) {
			ret = -EINVAL;
			break;
		}
		filp->f_pos += offset;
		ret = filp->f_pos;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}*/

static unsigned int globalmem_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct globalmem_dev *dev = filp->private_data;

	mutex_lock(&dev->mem_lock);
	
	poll_wait(filp, &dev->r_wait, wait);
	poll_wait(filp, &dev->w_wait, wait);

	if (dev->current_len != 0) {
		mask |= POLLIN | POLLRDNORM;
	}
	
	if (dev->current_len != GLOBALMEM_SIZE) {
		mask |= POLLOUT | POLLWRNORM;
	}

	mutex_unlock(&dev->mem_lock);
	return mask;
}

static const struct file_operations globalmem_fops = {
	.owner = THIS_MODULE,
	//.llseek = globalmem_llseek,
	.read = globalmem_read,
	.write = globalmem_write,
	.unlocked_ioctl = globalmem_ioctl,
	.open = globalmem_open,
	.release = globalmem_release,
	.poll = globalmem_poll,
	.fasync = globalmem_fasync,
};

static void globalmem_setup_cdev(struct globalmem_dev *dev, int index)
{
	printk(KERN_INFO "%s\n", __func__);
	int err, devno = MKDEV(globalmem_major, index);
	cdev_init(&dev->cdev, &globalmem_fops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	mutex_init(&dev->mem_lock);
	init_waitqueue_head(&dev->r_wait);
	init_waitqueue_head(&dev->w_wait);
	if (err)
	  printk(KERN_INFO "Error %d adding globalmem%d\n", err, index);
}

static int __init globalmem_init(void)
{
	printk(KERN_INFO "%s\n", __func__);
	int ret, i;
	dev_t devno = MKDEV(globalmem_major, 0);
	
	if (globalmem_major)
		ret = register_chrdev_region(devno, DEVICE_NUM, "globalmem");
	else {
		ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "globalmem");
		globalmem_major = MAJOR(devno);
	}
	if (ret < 0) {
		printk(KERN_INFO "%s:%d\n", __func__, ret);
		return ret;
	}

	globalmem_devp = kzalloc(sizeof(struct globalmem_dev) * DEVICE_NUM, GFP_KERNEL);
	if (!globalmem_devp) {
		ret = -ENOMEM;
		printk(KERN_INFO "%s:%s\n", __func__, "ENOMEM");
		goto fail_malloc;
	}
	for (i = 0; i < DEVICE_NUM; i++) {
		globalmem_setup_cdev(globalmem_devp + i, i);
	}
	return 0;

fail_malloc:
	unregister_chrdev_region(devno, DEVICE_NUM);
	printk(KERN_INFO "%s:%d\n", __func__, ret);
	return ret;
}

module_init(globalmem_init);

static void __exit globalmem_exit(void)
{
	int i;
	for (i = 0 ; i < DEVICE_NUM; i++) {
		cdev_del(&(globalmem_devp + i)->cdev);
	}
	kfree(globalmem_devp);
	unregister_chrdev_region(MKDEV(globalmem_major, 0), DEVICE_NUM);
}

module_exit(globalmem_exit);

MODULE_AUTHOR("Mr.zhao");
MODULE_LICENSE("GPL v2");
