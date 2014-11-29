obj-m = ktblmgr.o
KVERSION = $(shell uname -r)
all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
#	strip --strip-debug $(obj-m)
clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
