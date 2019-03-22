obj-m := joejoe.o
joejoe-objs := module.o core.o fops.o device.o patcher.o thread.o


EXTRA_CFLAGS=-I$(PWD)/../include
KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install: all
	sudo insmod joejoe.ko
	dmesg | tail -n 15

remove: clean
	sudo rmmod joejoe
	dmesg | tail -f

reboot: remove install
