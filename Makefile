.phony: all clean

CFLAGS=-Wall -Wextra -ansi -Wpedantic -g -O2

TARGETS=master

all: $(TARGETS)

%.s: %.c
	$(CC) $(CFLAGS) -S $< -o $@

clean:
	rm -vf $(TARGETS)
