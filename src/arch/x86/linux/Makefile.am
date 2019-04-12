ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -I$(top_srcdir)/src 
AM_MAKEFLAGS = -s

udevdir = /etc/udev/rules.d
initdir = @sysconfdir@/init.d

udev_DATA = cross_state_manager/83-cross_state_manager.rules

FLAGS=-DHAVE_CROSS_STATE

# Put the list of kernel modules to be built here.
# Then, modify install, uninstall, and clean hooks to 
# account for the generated modules.
# Finally, there should be one rule for each kernel module.
# Each module's subfolder must have its own Makefile
# (force added to the repository) which is used then by the
# kernel to build the module. CFLAGS must be explicitly reset
# to remove all the sh*t placed by autotools, which the kernel
# doesnt like. To pass parameters to the compilation of the module
# EXTRA_CFLAGS should be used.
all-local: cross_state_manager.ko intercept.ko schedule-hook.ko


# Per-module rules
cross_state_manager.ko: cross_state_manager/cross_state_manager.c
	$(MAKE) -C $(KERNEL_SRC) M=$(abs_srcdir)/cross_state_manager/ CFLAGS="" EXTRA_FLAGS="$(FLAGS)"

intercept.ko: intercept/intercept_page_fault.c intercept/my_page_fault.c
	cp cross_state_manager/Module.symvers intercept/
	$(MAKE) -C $(KERNEL_SRC) M=$(abs_srcdir)/intercept/ CFLAGS="" EXTRA_FLAGS="$(FLAGS)"

schedule-hook.ko: schedule-hook/schedule.c schedule-hook/hook.S schedule-hook/ld.c
	$(MAKE) -C $(KERNEL_SRC) M=$(abs_srcdir)/schedule-hook/ CFLAGS="" EXTRA_CFLAGS="$(FLAGS) \
											-DFTS_ADDR=$(FTS_ADDR)\
	       										-DFTS_ADDR_NEXT=$(FTS_ADDR_NEXT)"



# Autotools hooks to install, uninstall and clean
install-exec-hook: all-local
	$(INSTALL) $(abs_srcdir)/cross_state_manager/cross_state_manager.ko $(DESTDIR)$(KERNEL_MOD)/cross_state_manager.ko
	$(INSTALL) $(abs_srcdir)/intercept/intercept.ko $(DESTDIR)$(KERNEL_MOD)/intercept.ko
	$(INSTALL) $(abs_srcdir)/schedule-hook/schedule-hook.ko $(DESTDIR)$(KERNEL_MOD)/schedule-hook.ko
	depmod -a

uninstall-hook:
	$(RM) $(DESTDIR)$(KERNEL_MOD)/cross_state_manager.ko
	$(RM) $(DESTDIR)$(KERNEL_MOD)/intercept.ko
	$(RM) $(DESTDIR)$(KERNEL_MOD)/schedule-hook.ko
	depmod -a

clean-local:
	-$(MAKE) -C $(KERNEL_SRC) M=$(abs_srcdir)/cross_state_manager/ clean
	-$(MAKE) -C $(KERNEL_SRC) M=$(abs_srcdir)/intercept/ clean
	-$(MAKE) -C $(KERNEL_SRC) M=$(abs_srcdir)/schedule-hook/ clean

