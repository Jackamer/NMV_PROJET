#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <uapi/linux/sysinfo.h>
#include <linux/mm.h>
#include <linux/list.h>
#include "shell_mod.h"

#define BUFFER_SIZE 256

MODULE_DESCRIPTION("Module invite de commande");
MODULE_AUTHOR("Arnaud GUERMONT");
MODULE_LICENSE("GPL");

DECLARE_WAIT_QUEUE_HEAD(wait_queue);

struct kill_work {
	struct kill_data *data;
	struct work_struct task;
};

struct meminfo_work {
	struct sysinfo *meminfo;
	struct work_struct task;
};

struct lsmod_work {
	char *data;
	struct work_struct task;
};

struct kill_work *kill_worker;
struct meminfo_work *meminfo_worker;
struct lsmod_work *lsmod_worker;

static int kill_cond, meminfo_cond;

/* Device major number. */
static int major;
static char *name = "shell_mod";
static long device_ops(struct file *filp, unsigned int cmd, unsigned long arg);
const struct file_operations ops = {
	.unlocked_ioctl = device_ops
};

/* Send a signal to a process. */
static void kill(struct work_struct *task)
{
	struct kill_work *kw;
	struct pid *p;

	pr_debug("Doing kill in a workqueue!\n");
	kw = container_of(task, struct kill_work, task);
	p = find_get_pid(kw->data->upid);
	if(!p) {
		pr_err("Pid %d not found!\n", (int)kw->data->upid);
		goto err;
	}
	if(kill_pid(p, kw->data->sig, 0) != 0) {
		pr_err("Fail to send sig to pid %d!\n", (int)kw->data->upid);
		put_pid(p);
		goto err;
	}
	pr_debug("Signal successful send!\n");
	put_pid(p);

err:
	kfree(kw->data);
	if(!kill_cond) {
		pr_debug("Wake up kill!\n");
		kill_cond = 1;
		wake_up(&wait_queue);
	}
}

/* Get the memory state. */
static void get_meminfo(struct work_struct *task)
{
	struct meminfo_work *mw;

	pr_debug("Doing meminfo in a workqueue!\n");
	mw = container_of(task, struct meminfo_work, task);
	si_meminfo(mw->meminfo);
	if(!meminfo_cond) {
		pr_debug("Wake up meminfo!\n");
		meminfo_cond = 1;
		wake_up(&wait_queue);
	}
}

static void lsmod(struct work_struct *task)
{
	struct lsmod_work *lw;
	struct module *mod, *source;
	char *mod_info;
	char source_name[BUFFER_SIZE];
	int mem_free, char_append;

	pr_debug("Doing lsmod in a workqueue!\n");
	mem_free = BUFFER_SIZE;
	char_append = 0;
	lw = container_of(task, struct lsmod_work, task);
	/* modules is the head of the list of module. It's defined in /kernel/module.c */
	list_for_each_entry(mod, &modules, list) {
		mod_info = kmalloc(512, GFP_KERNEL);
		mem_free -= scnprintf(mod_info, STRING_SIZE,
				      "Module : %s Size : %8u Used by :%8u",
				      mod->name, mod->core_size, mod->init_size);
		if(mod->init_size > 0) {
			list_for_each_entry(source, &mod->source_list, source_list) {
				/* Clear the string. */
				memset(&source_name[0], 0, BUFFER_SIZE);
				char_append = scnprintf(source_name, BUFFER_SIZE, " Source mod : %s", source->name);
				strncat(mod_info, source_name, mem_free - char_append);
				mem_free -= char_append;
			}
		}
		strncat(mod_info, "\n", 1);
		strcat(lw->data, mod_info);
		kfree(mod_info);
	}
}
	
	

static long device_ops(struct file *filp, unsigned int cmd, unsigned long arg)
{	
	switch(cmd) {
	case KILL:
		pr_debug("kill ops!\n");
		kill_worker->data = kmalloc(sizeof(struct kill_data), GFP_KERNEL);
		copy_from_user(kill_worker->data, (void *)arg, sizeof(struct kill_data));
		pr_debug("Pid : %d\n", kill_worker->data->upid);
		schedule_work(&kill_worker->task);
		flush_work(&kill_worker->task);
		break;
	case KILL_ASYN:
		pr_debug("kill asynchrone ops!\n");
		/* Initialize the condition for the waitqueue. */
		kill_cond = 0;
		kill_worker->data = kmalloc(sizeof(struct kill_data), GFP_KERNEL);
		copy_from_user(kill_worker->data, (void *)arg, sizeof(struct kill_data));
		pr_debug("Pid : %d\n", kill_worker->data->upid);
		schedule_work(&kill_worker->task);
		wait_event(wait_queue, kill_cond);
		break;
	case MEMINFO:
		pr_debug("meminfo ops!\n");
		meminfo_worker->meminfo = kmalloc(sizeof(struct sysinfo), GFP_KERNEL);
		schedule_work(&meminfo_worker->task);
		flush_work(&meminfo_worker->task);
		copy_to_user((void *)arg, meminfo_worker->meminfo, sizeof(struct sysinfo));
		kfree(meminfo_worker->meminfo);
		break;
	case MEMINFO_ASYN:
		pr_debug("meminfo asynchrone ops!\n");
		/* Initialize the condition for the waitqueue. */
		meminfo_cond = 0;
		meminfo_worker->meminfo = kmalloc(sizeof(struct sysinfo), GFP_KERNEL);
		schedule_work(&meminfo_worker->task);
		wait_event(wait_queue, meminfo_cond);
		copy_to_user((void *)arg, meminfo_worker->meminfo, sizeof(struct sysinfo));
		kfree(meminfo_worker->meminfo);
		break;
	case LSMOD:
		pr_debug("lsmod ops!\n");
		lsmod_worker->data = kmalloc(STRING_SIZE, GFP_KERNEL);;
		schedule_work(&lsmod_worker->task);
		flush_work(&lsmod_worker->task);
		pr_debug("lsmod : %s", lsmod_worker->data);
		copy_to_user((char *)arg, lsmod_worker->data, STRING_SIZE);
		kfree(lsmod_worker->data);
		break;		
	default:
		pr_err("Unknown ops!\n");
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

	kill_worker = kmalloc(sizeof(struct kill_work), GFP_KERNEL);
	meminfo_worker = kmalloc(sizeof(struct meminfo_work), GFP_KERNEL);
	lsmod_worker = kmalloc(sizeof(struct lsmod_work), GFP_KERNEL);
	/* Init the struct for the workqueue for the first time. */
	INIT_WORK(&kill_worker->task, kill);
	INIT_WORK(&meminfo_worker->task, get_meminfo);
	INIT_WORK(&lsmod_worker->task, lsmod);
	return 0;
}

static void shell_mod_exit(void)
{
	kfree(kill_worker);
	kfree(meminfo_worker);
	kfree(lsmod_worker);
	
	unregister_chrdev(major, name);
	pr_debug("module unloaded\n");
}

module_init(shell_mod_init);
module_exit(shell_mod_exit);
