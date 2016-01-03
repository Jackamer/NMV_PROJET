#ifndef SHELL_MOD_H
#define SHELL_MOD_H

#include <linux/ioctl.h>

#define MAGIC_NUMBER 'N'

struct kill_data {
	int sig;
	pid_t upid;
};

#define KILL _IOR(MAGIC_NUMBER, 0, struct kill_data)
#define MEMINFO _IOWR(MAGIC_NUMBER, 0, struct sysinfo)

#endif
