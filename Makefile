obj-m := cntcp.o

KDIR := /lib/modules/3.10.0-1160.92.1.el7.x86_64/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
