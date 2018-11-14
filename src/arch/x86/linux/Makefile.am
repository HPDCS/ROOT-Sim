ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I$(top_srcdir)/src 
AM_MAKEFLAGS = -s

udevdir = /etc/udev/rules.d
initdir = @sysconfdir@/init.d
udev_DATA = rootsim/83-rootsim.rules

all-local: rootsim.ko


# Per-module rules
rootsim.ko: 
	$(MAKE) -C $(KERNEL_SRC) M=$(abs_srcdir)/rootsim/ CFLAGS=""

# Autotools hooks to install, uninstall and clean
install-exec-hook: all-local
	$(INSTALL) $(abs_srcdir)/rootsim/rootsim.ko $(DESTDIR)$(KERNEL_MOD)/rootsim.ko
	depmod -a

uninstall-hook:
	$(RM) $(DESTDIR)$(KERNEL_MOD)/rootsim.ko
	depmod -a

clean-local:
	-$(MAKE) -C $(KERNEL_SRC) M=$(abs_srcdir)/rootsim/ clean

