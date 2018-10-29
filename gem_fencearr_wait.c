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

static void test_syncobj_wait(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_exec_object2 obj;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_exec_fence fence = {
		.handle = syncobj_create(fd),
	};
	uint32_t *batch;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = (uint64_t)&obj;
	execbuf.buffer_count = 1;
	execbuf.flags = I915_EXEC_FENCE_ARRAY;
	execbuf.cliprects_ptr = to_user_pointer(&fence);
	execbuf.num_cliprects = 1;
	fence.flags = I915_EXEC_FENCE_SIGNAL;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);

	obj.relocs_ptr = (uint64_t)&reloc;
	obj.relocation_count = 1;
	memset(&reloc, 0, sizeof(reloc));

	batch = gem_mmap__wc(fd, obj.handle, 0, 4096, PROT_WRITE);
	gem_set_domain(fd, obj.handle,
		       I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

	reloc.target_handle = obj.handle; /* recurse */
	reloc.presumed_offset = 0;
	reloc.offset = sizeof(uint32_t);
	reloc.delta = 0;
	reloc.read_domains = I915_GEM_DOMAIN_COMMAND;
	reloc.write_domain = 0;

	int i = 0;
	batch[i] = MI_BATCH_BUFFER_START;
	batch[i] |= 1 << 8 | 1;
	batch[++i] = 0;
	batch[++i] = 0;
	i++;
	gem_execbuf(fd, &execbuf);

    /* Now wait upon the blocked engine */
	execbuf.flags = I915_EXEC_FENCE_ARRAY;
	execbuf.cliprects_ptr = to_user_pointer(&fence);
	execbuf.num_cliprects = 1;
	fence.flags = I915_EXEC_FENCE_WAIT;

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	gem_execbuf(fd, &execbuf);
	printf("wait...\n");
	sleep(1);
	assert(gem_bo_busy(fd, obj.handle));

	printf("signal...\n");
	*batch = MI_BATCH_BUFFER_END;
	__sync_synchronize();
	gem_sync(fd, obj.handle);
	assert(!gem_bo_busy(fd, obj.handle));

	syncobj_destroy(fd, fence.handle);
	gem_close(fd, obj.handle);
}

int main()
{
	const struct intel_execution_engine *e;
	int device = -1;
	device = drm_open_driver(DRIVER_INTEL);
	test_syncobj_wait(device);
	return 0;
}
