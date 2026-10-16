CC := $(HOME)/bbb_workspace/buildroot/output/host/bin/arm-linux-gcc
CFLAGS := -Wall -O2
TARGET := can_receiver
SRCS := can_receiver.c
BOARD := root@192.168.7.2

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $< -o $@

deploy: all
	scp -O $(TARGET) $(BOARD):/root/

clean:
	rm -rf $(TARGET)

.PHONY: all clean deploy
