obj-m += hello_world.o

default:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install: hello_world.ko
	install -d /lib/modules/$(shell uname -r)/kernel/misc/
	install -m 644 hello_world.ko /lib/modules/$(shell uname -r)/kernel/misc/
	depmod
