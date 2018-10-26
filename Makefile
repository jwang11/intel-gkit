#src = $(wildcard *.c)
targets = 	gem_exec_basic	\
			gem_exec_blt	\
			gem_tiled_wc	\
			gem_exec_gttfill\
			gem_exec_latency\
			gem_fence_busy


libsrc = gkit_lib.c

CC = gcc
all: $(targets)

gem_exec_basic: gem_exec_basic.c $(libsrc)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm

gem_exec_blt: gem_exec_blt.c $(libsrc)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm

gem_tiled_wc: gem_tiled_wc.c $(libsrc)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm

gem_exec_gttfill: gem_exec_gttfill.c $(libsrc)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm

gem_exec_latency: gem_exec_latency.c $(libsrc)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm

gem_fence_busy: gem_fence_busy.c $(libsrc)
	$(CC) -o $@ $^ -I/usr/include/libdrm -ldrm
.PHONY: clean

clean:
	@rm -f $(targets)
