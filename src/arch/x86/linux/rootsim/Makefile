obj-m := rootsim.o
rootsim-objs := fault.o fault_handler.o ioctl.o main.o pagetable.o scheduler.o timer.o idt.o
rootsim-objs += ime/core.o ime/irq.o ime/pmu.o ime/thread.o

CURRENT_PATH = $(shell pwd)
LINUX_KERNEL = $(shell uname -r)
LINUX_KERNEL_PATH = /lib/modules/$(LINUX_KERNEL)/build/
all:
			make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules

clean:
			make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean

