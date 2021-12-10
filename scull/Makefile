ifneq ($(KERNELRELEASE),)
	obj-m += scull.o

else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD  := $(shell pwd)

default:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

install: scull.ko
	install -d /lib/modules/$(shell uname -r)/kernel/misc/
	install -m 644 scull.ko /lib/modules/$(shell uname -r)/kernel/misc/
	depmod
endif
