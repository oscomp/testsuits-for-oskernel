/*
 * backfire - send signal back to caller
 *
 * Copyright (C) 2007  Carsten Emde <C.Emde@osadl.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include <linux/module.h>

#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/time.h>
#include <linux/smp_lock.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#define BACKFIRE_MINOR MISC_DYNAMIC_MINOR

static spinlock_t backfire_state_lock = SPIN_LOCK_UNLOCKED;
static int backfire_open_cnt; /* #times opened */
static int backfire_open_mode; /* special open modes */
static struct timeval sendtime; /* when the most recent signal was sent */
#define BACKFIRE_WRITE 1 /* opened for writing (exclusive) */
#define BACKFIRE_EXCL 2 /* opened with O_EXCL */

/*
 * These are the file operation function for user access to /dev/backfire
 */
static ssize_t
backfire_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	return snprintf(buf, count, "%d,%d\n", (int) sendtime.tv_sec,
		(int) sendtime.tv_usec);
}

static ssize_t
backfire_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int signo;
	struct pid *pid;

	if (sscanf(buf, "%d", &signo) >= 1) {
		if (signo > 0 && signo < 32) {
			pid = get_pid(task_pid(current));
			do_gettimeofday(&sendtime);
			kill_pid(pid, signo, 1);
		} else
			printk(KERN_ERR "Invalid signal no. %d\n", signo);
	}
	return strlen(buf);
}

static int
backfire_open(struct inode *inode, struct file *file)
{
	spin_lock(&backfire_state_lock);

	if ((backfire_open_cnt && (file->f_flags & O_EXCL)) ||
	    (backfire_open_mode & BACKFIRE_EXCL)) {
		spin_unlock(&backfire_state_lock);
		return -EBUSY;
	}

	if (file->f_flags & O_EXCL)
		backfire_open_mode |= BACKFIRE_EXCL;
	if (file->f_mode & 2)
		backfire_open_mode |= BACKFIRE_WRITE;
	backfire_open_cnt++;

	spin_unlock(&backfire_state_lock);

	return 0;
}

static int
backfire_release(struct inode *inode, struct file *file)
{
	spin_lock(&backfire_state_lock);

	backfire_open_cnt--;

	if (backfire_open_cnt == 1 && backfire_open_mode & BACKFIRE_EXCL)
		backfire_open_mode &= ~BACKFIRE_EXCL;
	if (file->f_mode & 2)
		backfire_open_mode &= ~BACKFIRE_WRITE;

	spin_unlock(&backfire_state_lock);

	return 0;
}

static struct file_operations backfire_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.open		= backfire_open,
	.read		= backfire_read,
	.write		= backfire_write,
	.release	= backfire_release,
};

static struct miscdevice backfire_dev = {
	BACKFIRE_MINOR,
	"backfire",
	&backfire_fops
};

static int __init backfire_init(void)
{
	int ret;

	ret = misc_register(&backfire_dev);
	if (ret)
		printk(KERN_ERR "backfire: can't register dynamic misc device\n");
	else
		printk(KERN_INFO "backfire driver misc device %d\n",
			backfire_dev.minor);
	return ret;
}

static void __exit backfire_exit(void)
{
	misc_deregister(&backfire_dev);
}

module_init(backfire_init);
module_exit(backfire_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carsten Emde <C.Emde@osadl.org>");
MODULE_DESCRIPTION("Send signal back to caller");
