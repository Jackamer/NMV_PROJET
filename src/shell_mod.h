#ifndef SHELL_MOD_H
#define SHELL_MOD_H

#include <linux/ioctl.h>

#define MAGIC_NUMBER 'N'

struct kill_data {
	int sig;
	pid_t upid;
};

#define KILL _IOR(MAGIC_NUMBER, 0, struct kill_data)
#define KILL_ASYN _IOR(MAGIC_NUMBER, 1, struct kill_data)
#define MEMINFO _IOWR(MAGIC_NUMBER, 2, struct sysinfo)
#define MEMINFO_ASYN _IOWR(MAGIC_NUMBER, 3, struct sysinfo)

#endif
