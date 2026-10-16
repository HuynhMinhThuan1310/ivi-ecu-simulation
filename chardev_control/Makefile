BR      ?= $(HOME)/bbb_workspace/buildroot
KDIR    ?= $(firstword $(wildcard $(BR)/output/build/linux-custom $(BR)/output/build/linux-[0-9]*))
CROSS   ?= $(BR)/output/host/bin/arm-linux-
BOARD   ?= root@192.168.7.2

obj-m += hello.o

all:
	@echo "KDIR = $(KDIR)"
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm CROSS_COMPILE=$(CROSS) modules
	$(CROSS)gcc test_ioctl.c -o test_ioctl

deploy: all
	scp -O hello.ko $(BOARD):/root/
	scp -O test_ioctl $(BOARD):/root/

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f test_ioctl

.PHONY: all deploy clean
