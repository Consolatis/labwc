// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/scaled-img-buffer.h"
#include "common/scaled-scene-buffer.h"
#include "img/img.h"
#include "node.h"

static struct wl_list cached_buffers = WL_LIST_INIT(&cached_buffers);

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
	struct scaled_img_buffer *self = scaled_buffer->data;
	if (!self->img) {
		wlr_log(WLR_ERROR, "Prevented rendering of destroyed lab_data buffer");
		return NULL;
	}
	struct lab_data_buffer *buffer = lab_img_render(self->img,
		self->width, self->height, self->padding, scale);
	return buffer;
}

static void
_destroy(struct scaled_scene_buffer *scaled_buffer)
{
	struct scaled_img_buffer *self = scaled_buffer->data;
	//wlr_log(WLR_ERROR, "scaled_img_buffer with img %p was destroyed", self->img);
	if (self->img) {
		wl_list_remove(&self->on_lab_img.destroy.link);
	}
	free(self);
}

static bool
_equal(struct scaled_scene_buffer *scaled_buffer_a,
	struct scaled_scene_buffer *scaled_buffer_b)
{
	struct scaled_img_buffer *a = scaled_buffer_a->data;
	struct scaled_img_buffer *b = scaled_buffer_b->data;

	if (!a->img || !b->img) {
		return false;
	}

	return a->img == b->img
		&& a->width == b->width
		&& a->height == b->height
		&& a->padding == b->padding;
}

static struct scaled_scene_buffer_impl impl = {
	.create_buffer = _create_buffer,
	.destroy = _destroy,
	.equal = _equal,
};

static void
handle_lab_img_destroy(struct wl_listener *listener, void *data)
{
	struct scaled_img_buffer *self =
		wl_container_of(listener, self, on_lab_img.destroy);
	//wlr_log(WLR_ERROR, "lab_img %p was destroyed", self->img);
	assert(self->img);
	self->img = NULL;
	wl_list_remove(&self->on_lab_img.destroy.link);
}


struct scaled_img_buffer *
scaled_img_buffer_create(struct wlr_scene_tree *parent, struct lab_img *img,
	int width, int height, int padding)
{
	struct scaled_scene_buffer *scaled_buffer = scaled_scene_buffer_create(
		parent, &impl, &cached_buffers, /* drop_buffer */ true);
	struct scaled_img_buffer *self = znew(*self);
	self->scaled_buffer = scaled_buffer;
	self->scene_buffer = scaled_buffer->scene_buffer;
	self->img = img;
	self->width = width;
	self->height = height;
	self->padding = padding;

	self->on_lab_img.destroy.notify = handle_lab_img_destroy;
	if (img) {
		//wlr_log(WLR_ERROR, "Adding initial destroy signal for lab_img %p", img);
		wl_signal_add(&img->events.destroy, &self->on_lab_img.destroy);
	}

	scaled_buffer->data = self;

	scaled_scene_buffer_request_update(scaled_buffer, width, height);

	return self;
}

void
scaled_img_buffer_update(struct scaled_img_buffer *self, struct lab_img *img,
	int width, int height, int padding)
{
	if (self->img != img) {
		if (self->img) {
			wl_list_remove(&self->on_lab_img.destroy.link);
		}
		if (img) {
			//wlr_log(WLR_ERROR, "Adding destroy signal for lab_img %p", img);
			wl_signal_add(&img->events.destroy, &self->on_lab_img.destroy);
		}
	}
	self->img = img;
	self->width = width;
	self->height = height;
	self->padding = padding;
	scaled_scene_buffer_request_update(self->scaled_buffer, width, height);
}

struct scaled_img_buffer *
scaled_img_buffer_from_node(struct wlr_scene_node *node)
{
	struct scaled_scene_buffer *scaled_buffer =
		node_scaled_scene_buffer_from_node(node);
	assert(scaled_buffer->impl == &impl);
	return scaled_buffer->data;
}
