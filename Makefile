obj-m := cntcp.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

MODULE_NAME := cntcp
KEY_FILE := kernel_sign_key.key
CERT_FILE := kernel_sign_cert.der

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

sign_module:
	/usr/src/linux-headers-$(shell uname -r)/scripts/sign-file sha256 $(KEY_FILE) $(CERT_FILE) $(MODULE_NAME).ko

generate_key_cert:
	openssl genpkey -algorithm RSA -out $(KEY_FILE) -aes256
	openssl req -new -x509 -key $(KEY_FILE) -out $(CERT_FILE) -outform der

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

clean:
	rm -f $(KEY_FILE) $(CERT_FILE)
	$(MAKE) -C $(KDIR) M=$(PWD) clean
