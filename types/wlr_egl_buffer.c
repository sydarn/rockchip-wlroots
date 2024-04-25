#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wlr/backend.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/egl.h>
#include <wlr/types/wlr_egl_buffer.h>
#include <wlr/util/log.h>
#include <xf86drm.h>
#include "render/drm_format_set.h"
#include "render/egl.h"
#include "util/shm.h"



static bool egl_buffer_get_dmabuf(struct wlr_buffer *wlr_buffer,
		struct wlr_dmabuf_attributes *attribs) {
	struct wlr_egl_buffer *buffer =
		wl_container_of(wlr_buffer, buffer, base);

	/* HACK: It's a guessed struct for mali_buffer_sharing extension */
	struct mali_buffer_sharing_info {
		int fd;
		int width;
		int height;
		int stride;
		uint32_t fourcc;
	};

	struct mali_buffer_sharing_info *info =
		wl_resource_get_user_data(buffer->resource);
	if (!info) {
		return false;
	}

	/* Check it carefully! */
	struct stat s;
	if (fstat(info->fd, &s) < 0 ||
		s.st_size < (wlr_buffer->width * wlr_buffer->height) ||
		info->width != wlr_buffer->width ||
		info->height != wlr_buffer->height ||
		info->stride < wlr_buffer->width) {
		return false;
	}

	attribs->width = wlr_buffer->width;
	attribs->height = wlr_buffer->height;
	attribs->modifier = DRM_FORMAT_MOD_INVALID;
	attribs->n_planes = 1;
	attribs->offset[0] = 0;

	attribs->stride[0] = info->stride;
	attribs->fd[0] = info->fd;
	attribs->format = info->fourcc;

	return true;
}

static void egl_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct wlr_egl_buffer *buffer =
		wl_container_of(wlr_buffer, buffer, base);
	wl_list_remove(&buffer->resource_destroy.link);
	wl_list_remove(&buffer->release.link);
	free(buffer);
}

static const struct wlr_buffer_impl egl_buffer_impl = {
	.get_dmabuf = egl_buffer_get_dmabuf,
	.destroy = egl_buffer_destroy,
};

struct wlr_egl_buffer *wlr_buffer_to_egl(struct wlr_buffer *buffer) {
	if (buffer->impl != &egl_buffer_impl)
		return NULL;

	return (struct wlr_egl_buffer *)buffer;
}

static void egl_buffer_resource_handle_destroy(
		struct wl_listener *listener, void *data) {
	struct wlr_egl_buffer *buffer =
		wl_container_of(listener, buffer, resource_destroy);

	buffer->resource = NULL;
	wl_list_remove(&buffer->resource_destroy.link);
	wl_list_init(&buffer->resource_destroy.link);

	wlr_buffer_drop(&buffer->base);
}

static void egl_buffer_handle_release(struct wl_listener *listener,
		void *data) {
	struct wlr_egl_buffer *buffer =
		wl_container_of(listener, buffer, release);
	if (buffer->resource != NULL) {
		wl_buffer_send_release(buffer->resource);
	}
}

static bool egl_buffer_resource_is_instance(struct wl_resource *resource) {
	struct wlr_egl *egl = wlr_egl_buffer_egl;
	if (!egl || !egl->exts.WL_bind_wayland_display)
		return false;

	EGLint fmt;
	return egl->procs.eglQueryWaylandBufferWL(egl->display,
		resource, EGL_TEXTURE_FORMAT, &fmt);
}

static struct wlr_buffer *egl_buffer_from_resource(struct wl_resource *resource) {
	struct wlr_egl *egl = wlr_egl_buffer_egl;
	if (!egl || !egl->exts.WL_bind_wayland_display)
		return NULL;

	struct wl_listener *resource_destroy_listener =
		wl_resource_get_destroy_listener(resource,
		egl_buffer_resource_handle_destroy);
	if (resource_destroy_listener != NULL) {
		struct wlr_egl_buffer *buffer =
			wl_container_of(resource_destroy_listener, buffer, resource_destroy);
		return &buffer->base;
	}

	EGLint fmt;
	int width, height;

	if (!egl->procs.eglQueryWaylandBufferWL(egl->display,
		resource, EGL_TEXTURE_FORMAT, &fmt))
		return NULL;

	if (!egl->procs.eglQueryWaylandBufferWL(egl->display,
		resource, EGL_WIDTH, &width))
		return NULL;

	if (!egl->procs.eglQueryWaylandBufferWL(egl->display,
		resource, EGL_HEIGHT, &height))
		return NULL;

	struct wlr_egl_buffer *buffer = calloc(1, sizeof(*buffer));
	if (!buffer)
		return NULL;

	wlr_buffer_init(&buffer->base, &egl_buffer_impl, width, height);

	buffer->resource = resource;

	buffer->resource_destroy.notify = egl_buffer_resource_handle_destroy;
	wl_resource_add_destroy_listener(resource, &buffer->resource_destroy);

	buffer->release.notify = egl_buffer_handle_release;
	wl_signal_add(&buffer->base.events.release, &buffer->release);

	switch (fmt) {
	case EGL_TEXTURE_RGB:
		buffer->has_alpha = false;
		break;
	case EGL_TEXTURE_RGBA:
	case EGL_TEXTURE_EXTERNAL_WL:
		buffer->has_alpha = true;
		break;
	default:
		wlr_log(WLR_ERROR, "Invalid or unsupported EGL buffer format");
		goto error;
	}

	return &buffer->base;
error:
	wlr_buffer_drop(&buffer->base);
	return NULL;
}

static struct wlr_buffer_resource_interface egl_buffer_resource_interface = {
	.name = "wlr_egl_buffer",
	.is_instance = egl_buffer_resource_is_instance,
	.from_resource = egl_buffer_from_resource,
};

void egl_buffer_register(struct wlr_egl *egl) {
	wlr_egl_buffer_egl = egl;
	wlr_buffer_register_resource_interface(&egl_buffer_resource_interface);
};

bool wlr_egl_bind_wl_display(struct wlr_egl *egl, struct wl_display *wl_display) {
	if (!egl->exts.WL_bind_wayland_display)
		return true;

	if (!egl->procs.eglBindWaylandDisplayWL(egl->display, wl_display))
		return false;

	egl->wl_display = wl_display;
	return true;
}

