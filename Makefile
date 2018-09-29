#src = $(wildcard *.c)

libsrc = gkit_lib.c
CC = gcc
all: gem_exec_basic gem_exec_blt

gem_exec_basic: gem_exec_basic.c $(libsrc)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm

gem_exec_blt: gem_exec_blt.c $(libsrc)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm

.PHONY: clean
clean:
	@rm -f gem_exec_basic gem_exec_blt
