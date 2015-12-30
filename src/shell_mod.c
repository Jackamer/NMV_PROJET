#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include "shell_mod.h"

MODULE_DESCRIPTION("Module invite de commande");
MODULE_AUTHOR("Arnaud GUERMONT");
MODULE_LICENSE("GPL");

struct kill_work {
	struct user_data *data;
	struct work_struct task;
};
struct kill_work *worker;

/* Device major number. */
static int major;
static char *name = "shell_mod";
static long device_ops(struct file *filp, unsigned int cmd, unsigned long arg);
const struct file_operations ops = {
	.unlocked_ioctl = device_ops
};

/* Send a signal to a process. */
static int kill(int sig, pid_t upid)
{
	struct pid *p;

	p = find_get_pid(upid);
	if(!p) {
		pr_err("Pid %d not found!\n", (int)upid);
		return ESRCH;		
	}
	if(kill_pid(p, sig, 0) != 0) {
		pr_err("Fail to send sig to pid %d!\n", (int)upid);
		put_pid(p);
		return EPERM;
	}
	pr_debug("Signal successful send!\n");
	put_pid(p);
	return 0;
}

static void asyn_kill(struct work_struct *task)
{
	struct kill_work *kw = container_of(task, struct kill_work, task);
	pr_debug("Doing kill in a work_queue!\n");
	kill(kw->data->sig, kw->data->upid);
	kfree(kw->data);
}

static long device_ops(struct file *filp, unsigned int cmd, unsigned long arg)
{	
	switch(cmd) {
	case KILL:
		pr_debug("kill ops!\n");
		worker->data = kmalloc(sizeof(struct user_data), GFP_KERNEL);
		copy_from_user(worker->data, (void *)arg, sizeof(struct user_data));
		pr_debug("Pid : %d\n", worker->data->upid);
		schedule_work(&worker->task);
		break;
	default:
		pr_err("Unknow ops!\n");
		return -ENOTTY;
	}

	return 0;
}

static int shell_mod_init(void)
{
	pr_debug("module loaded\n");

	major = register_chrdev(0, name, &ops);
	if(!major) {
		pr_err("Major not set!\n");
		return -1;
	}
	pr_info("Major device : %d\n", major);

	worker = kmalloc(sizeof(struct kill_work), GFP_KERNEL);
	/* Init the struct for the workqueue for the first time. */
	INIT_WORK(&worker->task, asyn_kill);
	return 0;
}

static void shell_mod_exit(void)
{
	kfree(worker);
	unregister_chrdev(major, name);
	pr_debug("module unloaded\n");
}

module_init(shell_mod_init);
module_exit(shell_mod_exit);
