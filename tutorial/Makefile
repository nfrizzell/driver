ifneq ($(KERNELRELEASE),)
	obj-m += hello_world.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD  := $(shell pwd)

default:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

install: hello_world.ko
	install -d /lib/modules/$(shell uname -r)/kernel/misc/
	install -m 644 hello_world.ko /lib/modules/$(shell uname -r)/kernel/misc/
	depmod
endif
