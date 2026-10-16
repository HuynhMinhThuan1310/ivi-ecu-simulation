BR      ?= $(HOME)/bbb_workspace/buildroot
KDIR    ?= $(firstword $(wildcard $(BR)/output/build/linux-custom $(BR)/output/build/linux-[0-9]*))
CROSS   ?= $(BR)/output/host/bin/arm-linux-
DTC     ?= $(BR)/output/host/bin/dtc
BOARD   ?= root@192.168.7.2

obj-m += speed_platform_driver.o

all:
	@echo "KDIR = $(KDIR)"
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm CROSS_COMPILE=$(CROSS) modules
	$(DTC) -O dtb -o speed_sensor.dtbo -b 0 -@ speed_sensor.dts

deploy: all
	scp -O speed_platform_driver.ko speed_sensor.dtbo $(BOARD):/root/

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f speed_sensor.dtbo

.PHONY: all deploy clean
