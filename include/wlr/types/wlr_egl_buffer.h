/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_EGL_BUFFER_H
#define WLR_TYPES_WLR_EGL_BUFFER_H

#include <stdint.h>
#include <sys/stat.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/egl.h>


/**
 * Part of mesa3d's EGL/eglmesaext.h
 */
#ifndef EGL_WL_bind_wayland_display
#define EGL_WL_bind_wayland_display 1

#define EGL_WAYLAND_BUFFER_WL           0x31D5 /* eglCreateImageKHR target */
#define EGL_WAYLAND_PLANE_WL            0x31D6 /* eglCreateImageKHR target */

#define EGL_TEXTURE_EXTERNAL_WL         0x31DA

struct wl_display;
struct wl_resource;
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglBindWaylandDisplayWL(EGLDisplay dpy, struct wl_display *display);
EGLAPI EGLBoolean EGLAPIENTRY eglUnbindWaylandDisplayWL(EGLDisplay dpy, struct wl_display *display);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryWaylandBufferWL(EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLUNBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, struct wl_display *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL) (EGLDisplay dpy, struct wl_resource *buffer, EGLint attribute, EGLint *value);

#endif

static struct wlr_egl *wlr_egl_buffer_egl;

struct wlr_egl_buffer {
	struct wlr_buffer base;
	struct wlr_egl *egl;
	bool has_alpha;

	struct wl_resource *resource;
	struct wl_listener resource_destroy;
	struct wl_listener release;
};

void egl_buffer_register(struct wlr_egl *egl);

struct wlr_egl_buffer *wlr_buffer_to_egl(struct wlr_buffer *buffer);

/**
 * Bind the wl_display of a Wayland compositor to an EGLDisplay.
 *
 * See:
 * https://registry.khronos.org/EGL/extensions/WL/EGL_WL_bind_wayland_display.txt
 */
bool wlr_egl_bind_wl_display(struct wlr_egl *egl, struct wl_display *wl_display);

/**
 * Creates an EGL image from the given EGL wl_buffer.
 *
 * See:
 * https://registry.khronos.org/EGL/extensions/WL/EGL_WL_bind_wayland_display.txt
 */
EGLImageKHR wlr_egl_create_image_from_eglbuf(struct wlr_egl *egl,
		struct wlr_egl_buffer *buffer);


#endif  /* WLR_TYPES_WLR_EGL_BUFFER_H */
