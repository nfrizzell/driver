#include <linux/init.h>
#include <linux/module.h>

static int init(void)
{
	return 0;
}

static void exit(void)
{
	
}

module_init(init);
module_exit(exit);
