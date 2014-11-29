obj-m += intercept.o
intercept-objs = my_page_fault.o intercept_page_fault.o

KVERSION = $(shell uname -r)
all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
#	strip --strip-debug $(obj-m)
clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
