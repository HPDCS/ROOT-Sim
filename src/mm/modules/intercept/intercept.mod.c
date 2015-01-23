#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x8479aeaa, "module_layout" },
	{ 0x799c50a, "param_set_ulong" },
	{ 0xea147363, "printk" },
	{ 0x56f494e0, "smp_call_function" },
	{ 0x93fca811, "__get_free_pages" },
	{ 0xa7f50dbf, "pv_cpu_ops" },
	{ 0x91766c09, "param_get_ulong" },
	{ 0x4302d0eb, "free_pages" },
	{ 0xdac4989d, "root_sim_page_fault" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=ktblmgr";

