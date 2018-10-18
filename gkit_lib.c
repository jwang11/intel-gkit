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

uint64_t nsec_elapsed(struct timespec *start)
{
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);
    if ((start->tv_sec | start->tv_nsec) == 0) {
        *start = now;
        return 0;
    }

    return ((now.tv_nsec - start->tv_nsec) +
        (uint64_t)NSEC_PER_SEC*(now.tv_sec - start->tv_sec));
}

int __gem_context_set_param(int fd, struct drm_i915_gem_context_param *p)
{
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, p))
		return -errno;

	errno = 0;
	return 0;
}

int __gem_context_set_priority(int fd, uint32_t ctx_id, int prio)
{
	struct drm_i915_gem_context_param p;

	memset(&p, 0, sizeof(p));
	p.ctx_id = ctx_id;
	p.size = 0;
	p.param = DRM_I915_CONTEXT_PARAM_PRIORITY;
	p.value = prio;

	return __gem_context_set_param(fd, &p);
}

int __gem_context_create(int fd, uint32_t *ctx_id)
{
       struct drm_i915_gem_context_create create;
       int err = 0;

       memset(&create, 0, sizeof(create));
       if (drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &create) == 0)
               *ctx_id = create.ctx_id;
       else
               err = -errno;

       errno = 0;
       return err;
}

uint32_t gem_context_create(int fd)
{
	uint32_t ctx_id;

	assert(__gem_context_create(fd, &ctx_id) == 0);
	assert(ctx_id != 0);

	return ctx_id;
}
/**
 * gem_context_destroy:
 * @fd: open i915 drm file descriptor
 * @ctx_id: i915 context id
 *
 * This wraps the CONTEXT_DESTROY ioctl, which is used to free a context.
 */
void gem_context_destroy(int fd, uint32_t ctx_id)
{
    struct drm_i915_gem_context_destroy destroy;

    memset(&destroy, 0, sizeof(destroy));
    destroy.ctx_id = ctx_id;

    drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_DESTROY, &destroy);
}

uint64_t gem_aperture_size(int fd)
{
    static uint64_t aperture_size = 0;

    if (aperture_size == 0) {
        struct drm_i915_gem_context_param p;

        memset(&p, 0, sizeof(p));
        p.param = 0x3;
        if (drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, &p) == 0) {
            aperture_size = p.value;
        } else {
            struct drm_i915_gem_get_aperture aperture;

            memset(&aperture, 0, sizeof(aperture));
            aperture.aper_size = 256*1024*1024;

            drmIoctl(fd, DRM_IOCTL_I915_GEM_GET_APERTURE, &aperture);
            aperture_size =  aperture.aper_size;
        }
    }

    return aperture_size;
}

int drm_get_card(void)
{
	char *name;
	int i, fd;

	for (i = 0; i < 16; i++) {
		int ret;

		ret = asprintf(&name, "/dev/dri/card%u", i);
		assert(ret != -1);

		fd = open(name, O_RDWR);
		free(name);

		if (fd == -1)
			continue;
		close(fd);
		return i;
	}

	printf("No intel gpu found\n");
	return -1;
}

void *__gem_mmap__gtt(int fd, uint32_t handle, uint64_t size, unsigned prot)
{
	struct drm_i915_gem_mmap_gtt mmap_arg;
	void *ptr;

	memset(&mmap_arg, 0, sizeof(mmap_arg));
	mmap_arg.handle = handle;
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_MMAP_GTT, &mmap_arg))
		return NULL;

	ptr = mmap(0, size, prot, MAP_SHARED, fd, mmap_arg.offset);
	return ptr;
}

void *gem_mmap__gtt(int fd, uint32_t handle, uint64_t size, unsigned prot)
{
	void *ptr = __gem_mmap__gtt(fd, handle, size, prot);
	assert(ptr);
	return ptr;
}

void *__gem_mmap__wc(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot)
{
    struct drm_i915_gem_mmap arg;

    memset(&arg, 0, sizeof(arg));
    arg.handle = handle;
    arg.offset = offset;
    arg.size = size;
    arg.flags = I915_MMAP_WC;
    if (drmIoctl(fd, DRM_IOCTL_I915_GEM_MMAP, &arg))
        return NULL;

    errno = 0;
    return (void *)arg.addr_ptr;
}

void *gem_mmap__wc(int fd, uint32_t handle, uint64_t offset, uint64_t size, unsigned prot)
{
    void *ptr = __gem_mmap__wc(fd, handle, offset, size, prot);
    assert(ptr);
    return ptr;
}

int gem_wait(int fd, uint32_t handle, int64_t *timeout_ns)
{
	struct drm_i915_gem_wait wait;
	int ret;

	memset(&wait, 0, sizeof(wait));
	wait.bo_handle = handle;
	wait.timeout_ns = timeout_ns ? *timeout_ns : -1;
	wait.flags = 0;

	ret = 0;
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_WAIT, &wait))
		ret = -errno;

	if (timeout_ns)
		*timeout_ns = wait.timeout_ns;

	return ret;
}

int __gem_set_domain(int fd, uint32_t handle, uint32_t read, uint32_t write)
{
	struct drm_i915_gem_set_domain set_domain;
	int err;

	memset(&set_domain, 0, sizeof(set_domain));
	set_domain.handle = handle;
	set_domain.read_domains = read;
	set_domain.write_domain = write;

	err = 0;
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain))
		err = -errno;

	return err;
}

void gem_set_domain(int fd, uint32_t handle, uint32_t read, uint32_t write)
{
	assert(__gem_set_domain(fd, handle, read, write) == 0);
}

void gem_sync(int fd, uint32_t handle)
{
	if (gem_wait(fd, handle, NULL))
		gem_set_domain(fd, handle,
			       I915_GEM_DOMAIN_GTT,
			       I915_GEM_DOMAIN_GTT);
	errno = 0;
}

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

int __gem_set_tiling(int fd, uint32_t handle, uint32_t tiling, uint32_t stride)
{
    struct drm_i915_gem_set_tiling st;
    int ret;

    /* The kernel doesn't know about these tiling modes, expects NONE */
    if (tiling == I915_TILING_Yf || tiling == I915_TILING_Ys)
        tiling = I915_TILING_NONE;

    memset(&st, 0, sizeof(st));
    do {
        st.handle = handle;
        st.tiling_mode = tiling;
        st.stride = tiling ? stride : 0;

        ret = ioctl(fd, DRM_IOCTL_I915_GEM_SET_TILING, &st);
    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
    if (ret != 0)
        return -errno;

    errno = 0;
    assert(st.tiling_mode == tiling);
    return 0;
}

void gem_set_tiling(int fd, uint32_t handle, uint32_t tiling, uint32_t stride)
{
    assert(__gem_set_tiling(fd, handle, tiling, stride) == 0);
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

int __gem_execbuf(int fd, struct drm_i915_gem_execbuffer2 *execbuf)
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
