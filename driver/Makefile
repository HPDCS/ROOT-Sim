obj-m := ime.o
ime-objs := ime_mod.o control.o irq_facility.o ime_fops.o ime_handler.o ime_device.o

KDIR ?= /lib/modules/$(shell uname -r)/build
# EXTRA_CFLAGS=-I$(PWD)/../include


all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	sudo insmod ime.ko
	dmesg | tail -n 15
remove:
	sudo rmmod ime
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	dmesg | tail -f

reboot:
	sudo rmmod ime
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	sudo insmod ime.ko
	dmesg | tail -n 15
