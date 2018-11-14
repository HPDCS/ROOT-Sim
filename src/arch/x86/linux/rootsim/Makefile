obj-m := rootsim.o
rootsim-objs := fault.o fault_handler.o ioctl.o main.o pagetable.o scheduler.o timer.o

CURRENT_PATH = $(shell pwd)
LINUX_KERNEL = $(shell uname -r)
LINUX_KERNEL_PATH = /usr/src/linux-headers-$(LINUX_KERNEL)
all:
			make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules

clean:
			make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean

