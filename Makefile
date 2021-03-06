.PHONY: all 
#.SECONDARY:

ifneq ($(KERNELRELEASE),)

obj-m := src/shell_mod.o
CFLAGS_shell_mod.o := -DDEBUG

else

  KERNELDIR ?= /lib/modules/$(shell uname -r)/build
  PWD := $(shell pwd)

all :
	$(info obj-m : $(obj-m))
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

endif
