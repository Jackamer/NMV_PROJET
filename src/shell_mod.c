#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/sched.h>

MODULE_DESCRIPTION("Module invite de commande");
MODULE_AUTHOR("Arnaud GUERMONT");
MODULE_LICENSE("GPL");

static int arg_pid;
static int arg_sig;

module_param(arg_pid, int, 0);
module_param(arg_sig, int, 0);

struct kill_work {
	int sig;
	pid_t upid;
	struct work_struct task;
};

struct kill_work *worker;

/*
 * Send a signal to a process.
 */
static int kill(int sig, pid_t upid)
{
	struct pid *p;

	p = find_get_pid(upid);
	if(!p)
		goto err_find;
	if(kill_pid(p, sig, 0) != 0)
		goto err_kill;
	pr_debug("Signal successful send!\n");
	put_pid(p);
	return 0;

err_find:
	pr_warn("Pid %d not found!\n", (int)upid);
	return ESRCH;
err_kill:
	pr_warn("Fail to send sig to pid %d!\n", (int)upid);
	put_pid(p);
	return EPERM;
}

static void asyn_kill(struct work_struct *task)
{
	struct kill_work *kw = container_of(task, struct kill_work, task);
	pr_debug("Doing kill in a work_queue!\n");
	kill(kw->sig, kw->upid);
}

static int shell_mod_init(void)
{
	pr_debug("module loaded\n");

	if(!arg_pid || !arg_sig) {
		pr_warn("Missing parameters!\n");
		return 0;
	}

	worker = kmalloc(sizeof(struct kill_work), GFP_KERNEL);
	worker->sig = arg_sig;
	worker->upid = arg_pid;

	/*
	 * Do the kill in a workqueues.
	 */
	INIT_WORK(&worker->task, asyn_kill);
	schedule_work(&worker->task);
	return 0;
}

static void shell_mod_exit(void)
{
	kfree(worker);
	pr_debug("module unloaded\n");
}

module_init(shell_mod_init);
module_exit(shell_mod_exit);
