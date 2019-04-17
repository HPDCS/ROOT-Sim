obj-m := rootsim.o
rootsim-objs := fault.o fault_handler.o ioctl.o main.o pagetable.o scheduler.o timer.o idt.o
rootsim-objs += ime/ime_mod.o ime/control.o ime/irq_facility.o ime/ime_fops.o ime/ime_handler.o ime/ime_device.o ime/ime_pebs.o

CURRENT_PATH = $(shell pwd)
LINUX_KERNEL = $(shell uname -r)
LINUX_KERNEL_PATH = /lib/modules/$(LINUX_KERNEL)/build/
all:
			make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules

clean:
			make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean

