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
#include <stdint.h>
#include <assert.h>
#include "gkit_lib.h"

extern const struct intel_execution_engine intel_execution_engines[];
static uint32_t batch_create(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	uint32_t handle;

	handle = gem_create(fd, 4096);
	gem_write(fd, handle, 0, &bbe, sizeof(bbe));

	return handle;
}

static void batch_fini(int fd, uint32_t handle)
{
	gem_sync(fd, handle);
	gem_close(fd, handle);
}

static void gtt(int fd, unsigned ring)
{
	struct drm_i915_gem_execbuffer2 *execbuf;
	struct drm_i915_gem_exec_object2 *exec;
	uint32_t handle;

	handle = gem_create(fd, 4096);

	gem_set_domain(fd, handle, I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);
	execbuf = gem_mmap__gtt(fd, handle, 4096, PROT_WRITE);
	exec = (struct drm_i915_gem_exec_object2 *)(execbuf + 1);
	gem_close(fd, handle);

	exec->handle = batch_create(fd);

	execbuf->buffers_ptr = (uint64_t)exec;
	execbuf->buffer_count = 1;
	execbuf->flags = ring;

	gem_execbuf(fd, execbuf);

	batch_fini(fd, exec->handle);
	munmap(execbuf, 4096);
}

static void readonly(int fd, unsigned ring)
{
    struct drm_i915_gem_execbuffer2 *execbuf;
    struct drm_i915_gem_exec_object2 exec;

    memset(&exec, 0, sizeof(exec));
    exec.handle = batch_create(fd);

    execbuf = mmap(NULL, 4096, PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(execbuf);

    execbuf->buffers_ptr = (uint64_t)&exec;
    execbuf->buffer_count = 1;
    execbuf->flags = ring;
    assert(mprotect(execbuf, 4096, PROT_READ) == 0);

    gem_execbuf(fd, execbuf);

    munmap(execbuf, 4096);

    batch_fini(fd, exec.handle);
}

static void noop(int fd, unsigned ring)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 exec;

	memset(&exec, 0, sizeof(exec));

	exec.handle = batch_create(fd);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = (uint64_t)&exec;
	execbuf.buffer_count = 1;
	execbuf.flags = ring;
	gem_execbuf(fd, &execbuf);

	batch_fini(fd, exec.handle);
}

int main(int argc, char **argv)
{
	const struct intel_execution_engine *e;
	int fd = drm_open_driver(DRIVER_INTEL);

	for (e = intel_execution_engines; e->name; e++) {
		noop(fd, e->exec_id | e->flags);
        printf("basic-%s\n", e->name);
        gtt(fd, e->exec_id | e->flags);
        printf("gtt-%s\n", e->name);
		readonly(fd, e->exec_id | e->flags);
        printf("readonly-%s\n", e->name);
		printf("\n");
	}
	close(fd);
	return 0;
}
