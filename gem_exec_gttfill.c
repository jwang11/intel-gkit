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

#define BATCH_SIZE (4096<<10)

struct batch {
	uint32_t handle;
	void *ptr;
};

static void submit(int fd,
		   struct drm_i915_gem_execbuffer2 *eb,
		   struct drm_i915_gem_relocation_entry *reloc,
		   struct batch *batches, unsigned int count)
{
	struct drm_i915_gem_exec_object2 obj;
	uint32_t batch[16];
	unsigned n;

	memset(&obj, 0, sizeof(obj));
	obj.relocs_ptr = (uint64_t)reloc;
	obj.relocation_count = 2;

	memset(reloc, 0, 2*sizeof(*reloc));
	reloc[0].offset = eb->batch_start_offset;
	reloc[0].offset += sizeof(uint32_t);
	reloc[0].delta = BATCH_SIZE - eb->batch_start_offset - 8;
	reloc[0].read_domains = I915_GEM_DOMAIN_INSTRUCTION;
	reloc[1].offset = eb->batch_start_offset;
	reloc[1].offset += 3*sizeof(uint32_t);
	reloc[1].read_domains = I915_GEM_DOMAIN_INSTRUCTION;

	n = 0;
	batch[n] = MI_STORE_DWORD_IMM;
		batch[n] |= 1 << 21;
		batch[n]++;
		batch[++n] = reloc[0].delta;/* lower_32_bits(address) */
		batch[++n] = 0; /* upper_32_bits(address) */
	batch[++n] = 0; /* lower_32_bits(value) */
	batch[++n] = 0; /* upper_32_bits(value) / nop */
	batch[++n] = MI_BATCH_BUFFER_END;

	eb->buffers_ptr = (uint64_t)&obj;
	for (unsigned i = 0; i < count; i++) {
		obj.handle = batches[i].handle;
		reloc[0].target_handle = obj.handle;
		reloc[1].target_handle = obj.handle;

		obj.offset = 0;
		reloc[0].presumed_offset = obj.offset;
		reloc[1].presumed_offset = obj.offset;

		memcpy(batches[i].ptr + eb->batch_start_offset,
		       batch, sizeof(batch));

		gem_execbuf(fd, eb);
	}
	/* As we have been lying about the write_domain, we need to do a sync */
	gem_sync(fd, obj.handle);
}

static void fillgtt(int fd, unsigned ring, int timeout)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_relocation_entry reloc[2];
	struct batch *batches;
	unsigned nengine;
	unsigned engine;
	uint64_t size;
	unsigned count;

	nengine = 0;

	size = gem_aperture_size(fd);
	if (size > 1ull<<32) /* Limit to 4GiB as we do not use allow-48b */
		size = 1ull << 32;

	count = size / BATCH_SIZE + 1;
	printf(" %luM ", size/1024/1024);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffer_count = 1;

	batches = calloc(count, sizeof(*batches));
	for (unsigned i = 0; i < count; i++) {
		batches[i].handle = gem_create(fd, BATCH_SIZE);
		batches[i].ptr =
			__gem_mmap__wc(fd, batches[i].handle,
				       0, BATCH_SIZE, PROT_WRITE);
		if (!batches[i].ptr) {
			printf("GTT ");
			batches[i].ptr =
				gem_mmap__gtt(fd, batches[i].handle,
						BATCH_SIZE, PROT_WRITE);
		}
	}

	uint64_t cycles = 0;
	execbuf.batch_start_offset = 0;
	execbuf.flags |= ring;
	until_timeout(timeout) {
		submit(fd, &execbuf, reloc, batches, count);
		for (unsigned i = 0; i < count; i++) {
			uint64_t offset, delta;
			offset = *(uint64_t *)(batches[i].ptr + reloc[1].offset);
			delta = *(uint64_t *)(batches[i].ptr + reloc[0].delta);
			assert(offset == delta);
		}
		cycles++;
	}
	printf("%llu cycles\n", (long long)cycles);

	for (unsigned i = 0; i < count; i++) {
		munmap(batches[i].ptr, BATCH_SIZE);
		gem_close(fd, batches[i].handle);
	}
}

int main()
{
	const struct intel_execution_engine *e;
	int device = -1;
	device = drm_open_driver(DRIVER_INTEL);
	printf("RENDER ENGINE:\t");
	fillgtt(device, I915_EXEC_RENDER, 1); /* just enough to run a single pass */
	return 0;
}
