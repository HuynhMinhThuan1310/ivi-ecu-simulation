CC := $(HOME)/bbb_workspace/buildroot/output/host/bin/arm-linux-gcc
CFLAGS := -Wall -O2
TARGET := map_navi
SRCS := map_navi.c
BOARD := root@192.168.7.2

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $< -o $@ -lm

deploy: all
	scp -O $(TARGET) $(BOARD):/root/

clean:
	rm -rf $(TARGET)

.PHONY: all clean deploy
