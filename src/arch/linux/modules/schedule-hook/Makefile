obj-m := schedule-hook.o
schedule-objs := hook.o schedule-hook.o ld.o
CURRENT_PATH = $(shell pwd)
LINUX_KERNEL = $(shell uname -r)
LINUX_KERNEL_PATH = /usr/src/linux-headers-$(LINUX_KERNEL)/
all:
		make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules
clean:
		make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean
