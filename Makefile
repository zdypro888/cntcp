obj-m := cntcp.o

KDIR := /lib/modules/5.15.0-76-generic/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
