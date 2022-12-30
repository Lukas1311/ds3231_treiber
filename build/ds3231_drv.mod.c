#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x7c30cdd7, "module_layout" },
	{ 0x572ff130, "no_llseek" },
	{ 0xd7b9544f, "i2c_del_driver" },
	{ 0x7586f234, "i2c_unregister_device" },
	{ 0x2009c6a2, "i2c_register_driver" },
	{ 0x4fee8c20, "i2c_new_client_device" },
	{ 0x6a758a2d, "i2c_get_adapter" },
	{ 0x5f754e5a, "memset" },
	{ 0x80ca5026, "_bin2bcd" },
	{ 0xbcab6ee6, "sscanf" },
	{ 0xae353d77, "arm_copy_from_user" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x51a910c0, "arm_copy_to_user" },
	{ 0x97255bdf, "strlen" },
	{ 0x314b20c8, "scnprintf" },
	{ 0xb6936ffe, "_bcd2bin" },
	{ 0x4dce47d8, "_raw_spin_trylock" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0x2ea9c462, "device_create" },
	{ 0x75d7cdf1, "__class_create" },
	{ 0xe50f8328, "cdev_add" },
	{ 0x322b4fd3, "cdev_init" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xb6c87ea1, "i2c_smbus_write_byte_data" },
	{ 0x476cfb1e, "i2c_smbus_read_byte_data" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xe247e3c3, "cdev_del" },
	{ 0xc69506dc, "class_destroy" },
	{ 0xe3a719e1, "device_destroy" },
	{ 0x92997ed8, "_printk" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
	{ 0xf7802486, "__aeabi_uidivmod" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("i2c:ds3231_drv");

MODULE_INFO(srcversion, "A3F33AC24C01FCE32CAC169");
