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
#ifndef __INTEL_GKIT_LIB_H
#define __INTEL_GKIT_LIB_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <xf86drm.h>
#include <i915_drm.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#define LOCAL_I915_CONTEXT_MAX_USER_PRIORITY	1023
#define LOCAL_I915_CONTEXT_DEFAULT_PRIORITY	0
#define LOCAL_I915_CONTEXT_MIN_USER_PRIORITY	-1023

#define DRM_I915_CONTEXT_PARAM_PRIORITY 0x6

#define MSEC_PER_SEC (1000)
#define USEC_PER_SEC (1000*MSEC_PER_SEC)
#define NSEC_PER_SEC (1000*USEC_PER_SEC)

uint64_t nsec_elapsed(struct timespec *start);

/**
 * seconds_elapsed:
 * @start: measure from this point in time
 *
 * A wrapper around nsec_elapsed that reports the approximate (8% error)
 * number of seconds since the start point.
 */
static inline uint32_t seconds_elapsed(struct timespec *start)
{
    return nsec_elapsed(start) >> 30;
}

/**
 * __gem_execbuf_wr:
 * @fd: open i915 drm file descriptor
 * @execbuf: execbuffer data structure
 *
 * This wraps the EXECBUFFER2_WR ioctl, which submits a batchbuffer for the gpu to
 * run. This is allowed to fail, with -errno returned.
 */
int __gem_execbuf_wr(int fd, struct drm_i915_gem_execbuffer2 *execbuf);

/**
 * gem_execbuf_wr:
 * @fd: open i915 drm file descriptor
 * @execbuf: execbuffer data structure
 *
 * This wraps the EXECBUFFER2_WR ioctl, which submits a batchbuffer for the gpu to
 * run.
 */
void gem_execbuf_wr(int fd, struct drm_i915_gem_execbuffer2 *execbuf);

/**
 * gem_bo_busy:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 *
 * This wraps the BUSY ioctl, which tells whether a buffer object is still
 * actively used by the gpu in a execbuffer.
 *
 * Returns: The busy state of the buffer object.
 */
bool gem_bo_busy(int fd, uint32_t handle);

/**
 * until_timeout:
 * @timeout: timeout in seconds
 *
 * Convenience macro loop to run the provided code block in a loop until the
 * timeout has expired. Of course when an individual execution takes too long,
 * the actual execution time could be a lot longer.
 *
 * The code block will be executed at least once.
 */
#define until_timeout(timeout) \
    for (struct timespec t__={}; seconds_elapsed(&t__) < (timeout); )

#define MI_BATCH_BUFFER_END (0xA << 23)
#define DRIVER_INTEL (1 << 0)
/*
 * Yf/Ys tiling
 *
 * Tiling mode in the I915_TILING_... namespace for new tiling modes which are
 * defined in the kernel. (They are not fenceable so the kernel does not need
 * to know about them.)
 *
 * They are to be used the the blitting routines below.
 */
#define I915_TILING_Yf  3
#define I915_TILING_Ys  4

struct intel_execution_engine {
	const char *name;
	const char *full_name;
	unsigned exec_id;
	unsigned flags;
};

int __gem_context_create(int fd, uint32_t *ctx_id);
/**
 * gem_context_create:
 * @fd: open i915 drm file descriptor
 *
 * This wraps the CONTEXT_CREATE ioctl, which is used to allocate a new
 * context. Note that similarly to gem_set_caching() this wrapper skips on
 * kernels and platforms where context support is not available.
 *
 * Returns: The id of the allocated context.
 */
uint32_t gem_context_create(int fd);

/**
 * gem_context_destroy:
 * @fd: open i915 drm file descriptor
 * @ctx_id: i915 context id
 *
 * This wraps the CONTEXT_DESTROY ioctl, which is used to free a context.
 */
void gem_context_destroy(int fd, uint32_t ctx_id);

/**
 * __gem_context_set_priority:
 * @fd: open i915 drm file descriptor
 * @ctx_id: i915 context id
 * @prio: desired context priority
 *
 * This function modifies priority property of the context.
 * It is used by the scheduler to decide on the ordering of requests submitted
 * to the hardware.
 *
 * Returns: An integer equal to zero for success and negative for failure
 */
int __gem_context_set_priority(int fd, uint32_t ctx_id, int prio);

/**
 * gem_aperture_size:
 * @fd: open i915 drm file descriptor
 *
 * Feature test macro to query the kernel for the total gpu aperture size.
 *
 * Returns: The total gtt address space size.
 */
uint64_t gem_aperture_size(int fd);

/**
 * drm_get_card:
 *
 * Get an i915 drm card index number for use in /dev or /sys. The minor index of
 * the legacy node is returned, not of the control or render node.
 *
 * Returns:
 * The i915 drm index or -1 on error
 */
int drm_get_card(void);

/**
 * drm_open_driver:
 * @chipset: OR'd flags for each chipset to search, eg. #DRIVER_INTEL
 *
 * Open a drm legacy device node. This function always returns a valid
 * file descriptor.
 *
 * Returns: a drm file descriptor
 */
int drm_open_driver(int chipset);

/**
 * __gem_mmap__wc:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @offset: offset in the gem buffer of the mmap arena
 * @size: size of the mmap arena
 * @prot: memory protection bits as used by mmap()
 *
 * This functions wraps up procedure to establish a memory mapping through
 * direct cpu access, bypassing the gpu and cpu caches completely and also
 * bypassing the GTT system agent (i.e. there is no automatic tiling of
 * the mmapping through the fence registers).
 *
 * Returns: A pointer to the created memory mapping, NULL on failure.
 */
void *__gem_mmap__wc(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot);

/**
 * gem_mmap__wc:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @offset: offset in the gem buffer of the mmap arena
 * @size: size of the mmap arena
 * @prot: memory protection bits as used by mmap()
 *
 * Like __gem_mmap__wc() except we assert on failure.
 *
 * Returns: A pointer to the created memory mapping
 */
void *gem_mmap__wc(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot);

/**
 * gem_mmap__gtt:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @size: size of the gem buffer
 * @prot: memory protection bits as used by mmap()
 *
 * Like __gem_mmap__gtt() except we assert on failure.
 *
 * Returns: A pointer to the created memory mapping
 */
void *gem_mmap__gtt(int fd, uint32_t handle, uint64_t size, unsigned prot);

/**
 * __gem_wait:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @timeout_ns: [in] time to wait, [out] remaining time (in nanoseconds)
 *
 * This functions waits for outstanding rendering to complete, upto
 * the timeout_ns. If no timeout_ns is provided, the wait is indefinite and
 * only returns upon an error or when the rendering is complete.
 */
int gem_wait(int fd, uint32_t handle, int64_t *timeout_ns);

/**
 * gem_set_domain:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @read: gem domain bits for read access
 * @write: gem domain bit for write access
 *
 * This wraps the SET_DOMAIN ioctl, which is used to control the coherency of
 * the gem buffer object between the cpu and gtt mappings. It is also use to
 * synchronize with outstanding rendering in general, but for that use-case
 * please have a look at gem_sync().
 */
void gem_set_domain(int fd, uint32_t handle, uint32_t read, uint32_t write);

/**
 * gem_sync:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 *
 * This functions waits for outstanding rendering to complete.
 */
void gem_sync(int fd, uint32_t handle);

/**
 * gem_set_tiling:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @tiling: tiling mode bits
 * @stride: stride of the buffer when using a tiled mode, otherwise must be 0
 *
 * This wraps the SET_TILING ioctl.
 */
void gem_set_tiling(int fd, uint32_t handle, uint32_t tiling, uint32_t stride);

/**
 * gem_close:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 *
 * This wraps the GEM_CLOSE ioctl, which to release a file-private gem buffer
 * handle.
 */
void gem_close(int fd, uint32_t handle);

/**
 * gem_create:
 * @fd: open i915 drm file descriptor
 * @size: desired size of the buffer
 *
 * This wraps the GEM_CREATE ioctl, which allocates a new gem buffer object of
 * @size.
 *
 * Returns: The file-private handle of the created buffer object
 */
uint32_t gem_create(int fd, uint64_t size);

/**
 * gem_write:
 * @fd: open i915 drm file descriptor
 * @handle: gem buffer object handle
 * @offset: offset within the buffer of the subrange
 * @buf: pointer to the data to write into the buffer
 * @length: size of the subrange
 *
 * This wraps the PWRITE ioctl, which is to upload a linear data to a subrange
 * of a gem buffer object.
 */
void gem_write(int fd, uint32_t handle, uint64_t offset, const void *buf, uint64_t length);

int __gem_execbuf(int fd, struct drm_i915_gem_execbuffer2 *execbuf);
/**
 * gem_execbuf:
 * @fd: open i915 drm file descriptor
 * @execbuf: execbuffer data structure
 *
 * This wraps the EXECBUFFER2 ioctl, which submits a batchbuffer for the gpu to
 * run.
 */
void gem_execbuf(int fd, struct drm_i915_gem_execbuffer2 *execbuf);

#endif  // __INTEL_GKIT_LIB_H
