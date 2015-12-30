#ifndef SHELL_MOD_H
#define SHELL_MOD_H

#include <linux/ioctl.h>

#define MAGIC_NUMBER 'N'

struct user_data {
	int sig;
	pid_t upid;
};

#define KILL _IOR(MAGIC_NUMBER, 0, struct user_data) 

#endif
