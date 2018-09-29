/*
 * Copyright © 2016 Intel Corporation
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
 */
#include <stdio.h>
#include "gkit_lib.h"

const struct intel_execution_engine intel_execution_engines[] = {
	{ "default", NULL, 0, 0 },
	{ "render", "rcs0", I915_EXEC_RENDER, 0 },
	{ "bsd", "vcs0", I915_EXEC_BSD, 0 },
	{ "bsd1", "vcs0", I915_EXEC_BSD, 1<<13 /*I915_EXEC_BSD_RING1*/ },
	{ "bsd2", "vcs1", I915_EXEC_BSD, 2<<13 /*I915_EXEC_BSD_RING2*/ },
	{ "blt", "bcs0", I915_EXEC_BLT, 0 },
	{ "vebox", "vecs0", I915_EXEC_VEBOX, 0 },
	{ NULL, 0, 0 }
};

static int __search_and_open(const char *base, int offset, unsigned int chipset)
{
	for (int i = 0; i < 16; i++) {
		char name[80];
		int fd;

		sprintf(name, "%s%u", base, i + offset);
		fd = open(name, O_RDWR);
		if (fd != -1)
			return fd;
	}

	return -1;
}

static int __open_driver(const char *base, int offset, unsigned int chipset)
{
	int fd;

	fd = __search_and_open(base, offset, chipset);
	if (fd != -1)
		return fd;

	return __search_and_open(base, offset, chipset);
}

static int __drm_open_driver(int chipset)
{
	return __open_driver("/dev/dri/card", 0, chipset);
}

int drm_open_driver(int chipset)
{
	return(__drm_open_driver(chipset));
}

void gem_close(int fd, uint32_t handle)
{
	struct drm_gem_close close_bo;

	memset(&close_bo, 0, sizeof(close_bo));
	close_bo.handle = handle;
	drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &close_bo);
}

static int __gem_create(int fd, uint64_t size, uint32_t *handle)
{
	struct drm_i915_gem_create create = {
		.size = size,
	};
	int err = 0;

	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create) == 0)
		*handle = create.handle;
	else
		err = -errno;

	errno = 0;
	return err;
}

uint32_t gem_create(int fd, uint64_t size)
{
	uint32_t handle;

	assert(__gem_create(fd, size, &handle)==0);

	return handle;
}

static int __gem_write(int fd, uint32_t handle, uint64_t offset, const void *buf, uint64_t length)
{
	struct drm_i915_gem_pwrite gem_pwrite;
	int err;

	memset(&gem_pwrite, 0, sizeof(gem_pwrite));
	gem_pwrite.handle = handle;
	gem_pwrite.offset = offset;
	gem_pwrite.size = length;
	gem_pwrite.data_ptr = (uint64_t)buf;

	err = 0;
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_PWRITE, &gem_pwrite))
		err = -errno;
	return err;
}

void gem_write(int fd, uint32_t handle, uint64_t offset, const void *buf, uint64_t length)
{
	assert(__gem_write(fd, handle, offset, buf, length)==0);
}

static int __gem_execbuf(int fd, struct drm_i915_gem_execbuffer2 *execbuf)
{
	int err = 0;
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_EXECBUFFER2, execbuf))
		err = -errno;
	errno = 0;
	return err;
}

void gem_execbuf(int fd, struct drm_i915_gem_execbuffer2 *execbuf)
{
	assert(__gem_execbuf(fd, execbuf)==0);
}
