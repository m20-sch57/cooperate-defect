.phony: all clean

CFLAGS=-Wall -Wextra -ansi -Wpedantic -g -O2

TARGETS=master master_npar

all: $(TARGETS)

%.s: %.c
	$(CC) $(CFLAGS) -S $< -o $@

%_npar: %.c
	$(CC) $(CFLAGS) -DNPAR   $<   -o $@

clean:
	rm -vf $(TARGETS)
