#include<linux/init.h>
#include<linux/module.h>

static char *test_name = "this is a module test";
module_param(test_name, charp, S_IRUGO);

static int __init hello_init(void)
{
	printk(KERN_INFO "Hello world enter\n");
	printk(KERN_INFO "test_name = %s\n", test_name);
	return 0;
}

module_init(hello_init);

static void __exit hello_exit(void)
{
	printk(KERN_INFO "Hello world exit\n");
}

module_exit(hello_exit);

MODULE_AUTHOR("zhaocc");
MODULE_LICENSE("GPL V2");
MODULE_DESCRIPTION("A simple hello world module");
MODULE_ALIAS("a simple module");
