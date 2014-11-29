# top makefile for ktblmgr
	obj-y := ktblmgr/ intercept/
CURRENT_PATH = $(shell pwd)
LINUX_KERNEL = $(shell uname -r)
LINUX_KERNEL_PATH = /usr/src/linux-headers-$(LINUX_KERNEL)
//LINUX_KERNEL_PATH = /usr/src/linux-headers-2.6.32-5-amd64
KDIR = $(LINUX_KERNEL_PATH) 
KVERSION=$(shell uname -r)

all:
	make -C /lib/modules/$(KVERSION)/build M=$(CURRENT_PATH)

install:
	insmod ./ktblmgr/ktblmgr.ko
	cd intercept ; ./load.sh ; cd ..
	mknod /dev/ktblmgr c 250 0 

uninstall:
	./intercept/unload.sh
	rmmod ./ktblmgr/ktblmgr.ko
	rm /dev/ktblmgr 
