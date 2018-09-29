src = $(wildcard *.c)

CC=gcc
gkit: $(src)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm

.PHONY: clean
clean:
	@rm -f gkit
