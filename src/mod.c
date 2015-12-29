#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/string.h>

MODULE_DESCRIPTION("Module invite de commande");
MODULE_AUTHOR("Arnaud GUERMONT");
MODULE_LICENSE("GPL");

static int arg_pid;
static int arg_sig;

module_param(arg_pid, int, 0);
module_param(arg_sig, int, 1); 

/*
 * Send a signal to a process.
 */
static int kill(int sig, pid_t upid)
{
	struct pid *p;

	p = find_get_pid(upid);
	if(!p)
		goto err_find;
	if(!kill_pid(p, sig, 0))
		goto err_kill;
	return 0;
	
err_find:
	pr_debug("Pid %d not found!\n", (int)upid);
	return ESRCH;
err_pid:
	pr_debug("Fail to send sig to pid %d!\n", (int)upid);
	return EPERM;
}

static int mod_init(void)
{
	pr_debug("module loaded\n");
	if(!arg_pid || !arg_sig)
		return EINVAL;
	kill(arg_sig, arg_pid);
	return 0;
}

static void mod_exit(void)
{
	pr_debug("module unloaded\n");
}

module_init(mod_init);
module_exit(mod_exit);
