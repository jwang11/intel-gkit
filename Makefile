#src = $(wildcard *.c)
targets = 	gem_exec_basic	\
			gem_exec_blt	\
			gem_tiled_wc

libsrc = gkit_lib.c

CC = gcc
all: $(targets)

gem_exec_basic: gem_exec_basic.c $(libsrc)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm

gem_exec_blt: gem_exec_blt.c $(libsrc)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm

gem_tiled_wc: gem_tiled_wc.c $(libsrc)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm

.PHONY: clean
clean:
	@rm -f $(targets)
