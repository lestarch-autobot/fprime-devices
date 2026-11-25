# Pathfinder probe: needs libusb-1.0 (Ubuntu: apt install libusb-1.0-0-dev pkg-config)
CFLAGS ?= -O2 -g -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion
CFLAGS += $(shell pkg-config --cflags libusb-1.0)
LDLIBS += $(shell pkg-config --libs libusb-1.0)

u3vprobe: u3vprobe.c u3v.h
	$(CC) $(CFLAGS) -o $@ u3vprobe.c $(LDLIBS)

clean:
	rm -f u3vprobe
