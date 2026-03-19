#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hamza Trabelsi");
MODULE_DESCRIPTION("AI-OS first kernel module");

static int __init hello_init(void) {
    printk(KERN_INFO "AI-OS: Kernel module loaded\n");
    printk(KERN_INFO "AI-OS: We are inside the kernel now\n");
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "AI-OS: Kernel module unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);