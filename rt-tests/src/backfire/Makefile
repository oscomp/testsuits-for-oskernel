# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language
ifneq ($(KERNELRELEASE),)
obj-m := backfire.o

# otherwise we were called directly from the command
# line; invoke the kernel build system.
else
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

modules modules_install clean::
	make -C $(KERNELDIR) M=$(CURDIR) $@

clean::
	rm -f *.o Module.markers modules.order

endif
