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

#include "gkit_lib.h"
#include "intel_reg.h"

#define HANG 0x1
#define NONBLOCK 0x2
#define WAIT 0x4
static int syncobj_to_sync_file(int fd, uint32_t handle)
{
    struct local_syncobj_handle {
        uint32_t handle;
        uint32_t flags;
        int32_t fd;
        uint32_t pad;
    } arg;

    memset(&arg, 0, sizeof(arg));
    arg.handle = handle;
    arg.flags = 1 << 0; /* EXPORT_SYNC_FILE */
    if (drmIoctl(fd, DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD, &arg))
        arg.fd = -errno;

    errno = 0;
    return arg.fd;
}

static bool syncobj_busy(int fd, uint32_t handle)
{
    bool result;
    int sf;

    sf = syncobj_to_sync_file(fd, handle);
    result = poll(&(struct pollfd){sf, POLLIN}, 1, 0) == 0;
    close(sf);

    return result;
}

static void test_syncobj_signal(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_fence fence = {
		.handle = syncobj_create(fd),
	};

    /* Check that the syncobj is signaled only when our request/fence is */

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = (uint64_t)&obj;
	execbuf.buffer_count = 1;
	execbuf.flags = I915_EXEC_FENCE_ARRAY;
	execbuf.cliprects_ptr = (uint64_t)&fence;
	execbuf.num_cliprects = 1;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	fence.flags = I915_EXEC_FENCE_SIGNAL;

	struct timespec start;
	clock_gettime(CLOCK_REALTIME, &start);
	gem_execbuf(fd, &execbuf);

	assert(gem_bo_busy(fd, obj.handle));
	assert(syncobj_busy(fd, fence.handle));
	printf("syncobj busy\n");

	gem_sync(fd, obj.handle);
	assert(!gem_bo_busy(fd, obj.handle));
	assert(!syncobj_busy(fd, fence.handle));
	printf("syncobj idle\n");

	gem_close(fd, obj.handle);
	syncobj_destroy(fd, fence.handle);
}

int main()
{
	const struct intel_execution_engine *e;
	int device = -1;
	device = drm_open_driver(DRIVER_INTEL);
	test_syncobj_signal(device);
	return 0;
}
