BR      ?= $(HOME)/bbb_workspace/buildroot
KDIR    ?= $(firstword $(wildcard $(BR)/output/build/linux-custom $(BR)/output/build/linux-[0-9]*))
CROSS   ?= $(BR)/output/host/bin/arm-linux-
BOARD   ?= root@192.168.7.2

obj-m += speed_driver.o

all:
	@echo "KDIR = $(KDIR)"
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm CROSS_COMPILE=$(CROSS) modules
	$(CROSS)gcc test_speed.c -o test_speed

deploy: all
	scp -O speed_driver.ko $(BOARD):/root/
	scp -O test_speed $(BOARD):/root/

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f test_speed

.PHONY: all deploy clean
