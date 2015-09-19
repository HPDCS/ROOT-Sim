ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -I$(abs_srcdir)

udevdir = /etc/udev/rules.d
initdir = @sysconfdir@/init.d

udev_DATA = cross_state_manager/83-cross_state_manager.rules

cross_state_manager/cross_state_manager.ko: cross_state_manager/cross_state_manager.c
	test -f $(abs_srcdir)/cross_state_manager/Makefile || $(LN_S) $(abs_srcdir)/cross_state_manager/Makefile $(abs_srcdir)/cross_state_manager/
	test -f $(abs_srcdir)/cross_state_manager/cross_state_manager.c || $(LN_S) $(abs_srcdir)/cross_state_manager/cross_state_manager.c $(abs_srcdir)/cross_state_manager/
	test -f $(abs_srcdir)/cross_state_manager/cross_state_manager.h || $(LN_S) $(abs_srcdir)/cross_state_manager/cross_state_manager.h $(abs_srcdir)/cross_state_manager/
	$(MAKE) -C $(KERNEL_SRC) M=$(abs_srcdir)/cross_state_manager EXTRA_CFLAGS=""
all-local: cross_state_manager/cross_state_manager.ko

install-exec-hook: cross_state_manager/cross_state_manager.ko
	test -z "$(DESTDIR)$(KERNEL_MOD)" || $(MKDIR_P) "$(DESTDIR)$(KERNEL_MOD)"
	$(INSTALL) $(abs_srcdir)/cross_state_manager/cross_state_manager.ko $(DESTDIR)$(KERNEL_MOD)/cross_state_manager.ko
	depmod -a

uninstall-hook:
	$(RM) $(DESTDIR)$(KERNEL_MOD)/cross_state_manager.ko
	depmod -a

clean-local:
	-$(MAKE) -C $(KERNEL_SRC) M=$(abs_srcdir)/cross_state_manager clean

