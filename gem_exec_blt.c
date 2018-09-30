/*
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "gkit_lib.h"

#define OBJECT_SIZE 16384

#define COPY_BLT_CMD		(2<<29|0x53<<22|0x6)
#define BLT_WRITE_ALPHA		(1<<21)
#define BLT_WRITE_RGB		(1<<20)

static int gem_linear_blt(int fd,
			  uint32_t *batch,
			  uint32_t src,
			  uint32_t dst,
			  uint32_t length,
			  struct drm_i915_gem_relocation_entry *reloc)
{
	uint32_t *b = batch;
	int height = length / (16 * 1024);

	assert(height <= (1 << 16));

	if (height) {
		int i = 0;
		b[i++] = COPY_BLT_CMD | BLT_WRITE_ALPHA | BLT_WRITE_RGB;
		b[i-1]+=2;
		b[i++] = 0xcc << 16 | 1 << 25 | 1 << 24 | (16*1024);
		b[i++] = 0;
		b[i++] = height << 16 | (4*1024);
		b[i++] = 0;
		reloc->offset = (b-batch+4) * sizeof(uint32_t);
		reloc->delta = 0;
		reloc->target_handle = dst;
		reloc->read_domains = I915_GEM_DOMAIN_RENDER;
		reloc->write_domain = I915_GEM_DOMAIN_RENDER;
		reloc->presumed_offset = 0;
		reloc++;
		b[i++] = 0;

		b[i++] = 0;
		b[i++] = 16*1024;
		b[i++] = 0;
		reloc->offset = (b-batch+7) * sizeof(uint32_t);
		reloc->offset += sizeof(uint32_t);
		reloc->delta = 0;
		reloc->target_handle = src;
		reloc->read_domains = I915_GEM_DOMAIN_RENDER;
		reloc->write_domain = 0;
		reloc->presumed_offset = 0;
		reloc++;
		b[i++] = 0;

		b += i;
		length -= height * 16*1024;
	}

	if (length) {
		int i = 0;
		b[i++] = COPY_BLT_CMD | BLT_WRITE_ALPHA | BLT_WRITE_RGB;
		b[i-1]+=2;
		b[i++] = 0xcc << 16 | 1 << 25 | 1 << 24 | (16*1024);
		b[i++] = height << 16;
		b[i++] = (1+height) << 16 | (length / 4);
		b[i++] = 0;
		reloc->offset = (b-batch+4) * sizeof(uint32_t);
		reloc->delta = 0;
		reloc->target_handle = dst;
		reloc->read_domains = I915_GEM_DOMAIN_RENDER;
		reloc->write_domain = I915_GEM_DOMAIN_RENDER;
		reloc->presumed_offset = 0;
		reloc++;
		b[i++] = 0;

		b[i++] = height << 16;
		b[i++] = 16*1024;
		b[i++] = 0;
		reloc->offset = (b-batch+7) * sizeof(uint32_t);
		reloc->offset += sizeof(uint32_t);
		reloc->delta = 0;
		reloc->target_handle = src;
		reloc->read_domains = I915_GEM_DOMAIN_RENDER;
		reloc->write_domain = 0;
		reloc->presumed_offset = 0;
		reloc++;
		b[i++] = 0;

		b += i;
	}

	b[0] = MI_BATCH_BUFFER_END;
	b[1] = 0;

	return (b+2 - batch) * sizeof(uint32_t);
}

static double elapsed(const struct timeval *start,
		      const struct timeval *end)
{
	return 1e6*(end->tv_sec - start->tv_sec) + (end->tv_usec - start->tv_usec);
}

static const char *bytes_per_sec(char *buf, double v)
{
	const char *order[] = {
		"",
		"KiB",
		"MiB",
		"GiB",
		"TiB",
		"PiB",
		NULL,
	}, **o = order;

	while (v > 1024 && o[1]) {
		v /= 1024;
		o++;
	}
	sprintf(buf, "%.1f%s/s", v, *o);
	return buf;
}

static int dcmp(const void *A, const void *B)
{
	const double *a = A, *b = B;
	if (*a < *b)
		return -1;
	else if (*a > *b)
		return 1;
	else
		return 0;
}

static void run(int object_size)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 exec[3];
	struct drm_i915_gem_relocation_entry reloc[4];
	uint32_t buf[20];
	uint32_t handle, src, dst;
	int fd, len, count;
	int ring;

	fd = drm_open_driver(DRIVER_INTEL);

	handle = gem_create(fd, 4096);

	src = gem_create(fd, object_size);
	dst = gem_create(fd, object_size);

	len = gem_linear_blt(fd, buf, 0, 1, object_size, reloc);
	gem_write(fd, handle, 0, buf, len);

	memset(exec, 0, sizeof(exec));
	exec[0].handle = src;
	exec[1].handle = dst;

	exec[2].handle = handle;
	exec[2].relocation_count = len > 56 ? 4 : 2;
	exec[2].relocs_ptr = (uint64_t)reloc;

	ring = I915_EXEC_BLT;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = (uint64_t)exec;
	execbuf.buffer_count = 3;
	execbuf.batch_len = len;
	execbuf.flags = ring;
	execbuf.flags |= I915_EXEC_HANDLE_LUT;
	execbuf.flags |= I915_EXEC_NO_RELOC;

	if (__gem_execbuf(fd, &execbuf)) {
		len = gem_linear_blt(fd, buf, src, dst, object_size, reloc);
		assert(len == execbuf.batch_len);
		gem_write(fd, handle, 0, buf, len);
		execbuf.flags = ring;
		gem_execbuf(fd, &execbuf);
	}
	gem_sync(fd, handle);

	struct timeval start, end;

	gettimeofday(&start, NULL);
	for (int loop = 0; loop < 1<<12; loop++)
		gem_execbuf(fd, &execbuf);
	gem_sync(fd, handle);
	gettimeofday(&end, NULL);
	double duration = elapsed(&start, &end);
	printf("Time to blt %d bytes:	%7.3fµs, %s\n",
		 object_size, duration,
		 bytes_per_sec((char *)buf, object_size/duration*1e6));
	fflush(stdout);
	gem_close(fd, handle);
	close(fd);
}

static int sysfs_read(const char *name)
{
	char buf[4096];
	int sysfd;
	int len;

	sprintf(buf, "/sys/class/drm/card%d/%s",
		drm_get_card(), name);
	sysfd = open(buf, O_RDONLY);
	if (sysfd < 0)
		return -1;

	len = read(sysfd, buf, sizeof(buf)-1);
	close(sysfd);
	if (len < 0)
		return -1;

	buf[len] = '\0';
	return atoi(buf);
}

static int sysfs_write(const char *name, int value)
{
	char buf[4096];
	int sysfd;
	int len;

	sprintf(buf, "/sys/class/drm/card%d/%s",
		drm_get_card(), name);
	sysfd = open(buf, O_WRONLY);
	if (sysfd < 0)
		return -1;

	len = sprintf(buf, "%d", value);
	len = write(sysfd, buf, len);
	close(sysfd);

	if (len < 0)
		return len;

	return 0;
}

static void set_auto_freq(void)
{
	int min = sysfs_read("gt_RPn_freq_mhz");
	int max = sysfs_read("gt_RP0_freq_mhz");
	if (max <= min)
		return;

	printf("Setting to %d-%dMHz auto\n", min, max);
	sysfs_write("gt_min_freq_mhz", min);
	sysfs_write("gt_max_freq_mhz", max);
}

static void set_min_freq(void)
{
	int min = sysfs_read("gt_RPn_freq_mhz");
	printf("Setting to %dMHz\n", min);
	assert(sysfs_write("gt_min_freq_mhz", min) == 0);
	assert(sysfs_write("gt_max_freq_mhz", min) == 0);
}

static void set_max_freq(void)
{
	int max = sysfs_read("gt_RP0_freq_mhz");
	printf("Setting to %dMHz\n", max);
	assert(sysfs_write("gt_max_freq_mhz", max) == 0);
	assert(sysfs_write("gt_min_freq_mhz", max) == 0);
}


int main(int argc, char **argv)
{
	const struct {
		const char *suffix;
		void (*func)(void);
	} rps[] = {
		{ "-auto", set_auto_freq },
		{ "-min", set_min_freq },
		{ "-max", set_max_freq },
		{ NULL, NULL },
	}, *r;
	int min = -1, max = -1;
	int i;

	min = sysfs_read("gt_min_freq_mhz");
	max = sysfs_read("gt_max_freq_mhz");

	for (r = rps; r->suffix; r++) {
		r->func();
		run(OBJECT_SIZE);
		printf("\n");
	}

	sysfs_write("gt_min_freq_mhz", min);
	sysfs_write("gt_max_freq_mhz", max);
}
