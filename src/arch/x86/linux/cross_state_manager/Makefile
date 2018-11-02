obj-m := cross_state_manager.o
crossstate-objs = cross_state_manager.o built_in.o 83-cross_state_manager.o
CURRENT_PATH = $(shell pwd)
LINUX_KERNEL = $(shell uname -r)
LINUX_KERNEL_PATH = /usr/src/linux-headers-$(LINUX_KERNEL)/
all:
		make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules
clean:
		make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean
