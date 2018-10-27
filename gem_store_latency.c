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
#include "intel_reg.h"

#define _TIMES 128

static uint32_t create_highest_priority(int fd)
{
	uint32_t ctx = gem_context_create(fd);
	__gem_context_set_priority(fd, ctx, LOCAL_I915_CONTEXT_MAX_USER_PRIORITY);
	return ctx;
}

static uint64_t latencies[_TIMES];
static uint64_t total_latency;

static void store(int fd, unsigned ring, uint32_t target, uint32_t ctx_id, unsigned offset_value)
{
	const int SCRATCH = 0;
	const int BATCH = 1;
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_relocation_entry reloc;
	struct drm_i915_gem_execbuffer2 execbuf;
	uint32_t batch[16];
	int i;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = (uint64_t)obj;
	execbuf.buffer_count = 2;
	execbuf.flags = ring;
	execbuf.rsvd1 = ctx_id;

	memset(obj, 0, sizeof(obj));
	obj[SCRATCH].handle = target;

	obj[BATCH].handle = gem_create(fd, 4096);
	obj[BATCH].relocs_ptr = (uint64_t)&reloc;
	obj[BATCH].relocation_count = 1;
	memset(&reloc, 0, sizeof(reloc));

	i = 0;
	reloc.target_handle = obj[SCRATCH].handle;
	reloc.presumed_offset = -1;
	reloc.offset = sizeof(uint32_t) * (i + 1);
	reloc.delta = sizeof(uint32_t) * offset_value;
	reloc.read_domains = I915_GEM_DOMAIN_INSTRUCTION;
	reloc.write_domain = I915_GEM_DOMAIN_INSTRUCTION;
	batch[i] = MI_STORE_DWORD_IMM;
	batch[++i] = reloc.delta;
	batch[++i] = 0;
	batch[++i] = offset_value;
	batch[++i] = MI_BATCH_BUFFER_END;
	gem_write(fd, obj[BATCH].handle, 0, batch, sizeof(batch));
	gem_execbuf(fd, &execbuf);
	gem_close(fd, obj[BATCH].handle);
}

static void get_latency(int fd, unsigned ring, uint32_t ctx_id, int num)
{
	struct timespec start;
	uint32_t handle = gem_create(fd, 4096);
	clock_gettime(CLOCK_REALTIME, &start);
	store(fd, ring, handle, ctx_id, num);
	gem_sync(fd, handle);
	latencies[num] = nsec_elapsed(&start);
	total_latency += latencies[num];
	gem_close(fd, handle);
}

static uint64_t calc_average_latency(int fd)
{
	uint32_t ctx_id = create_highest_priority(fd);
	total_latency = 0;
	for (int i = 0; i < _TIMES; i++) {
		get_latency(fd, I915_EXEC_RENDER, ctx_id, i);
	}
	gem_context_destroy(fd, ctx_id);
	return total_latency/_TIMES;	
}

int main(int argc, char **argv)
{
	const struct intel_execution_engine *e;
	int fd = drm_open_driver(DRIVER_INTEL);
	/* make GPU warm up */
	calc_average_latency(fd);

	/* measure latency formally */
	for (int i = 0; i < 16; i++) {
		printf("latency: %4.1fms\n", calc_average_latency(fd)/1000.0);
		sleep(1);
	}
	close(fd);
	return 0;
}
