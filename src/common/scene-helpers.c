// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include "common/scene-helpers.h"
#include "labwc.h"

struct wlr_surface *
lab_wlr_surface_from_node(struct wlr_scene_node *node)
{
	struct wlr_scene_buffer *buffer;
	struct wlr_scene_surface *scene_surface;

	if (node && node->type == WLR_SCENE_NODE_BUFFER) {
		buffer = wlr_scene_buffer_from_node(node);
		scene_surface = wlr_scene_surface_try_from_buffer(buffer);
		if (scene_surface) {
			return scene_surface->surface;
		}
	}
	return NULL;
}

struct wlr_scene_node *
lab_wlr_scene_get_prev_node(struct wlr_scene_node *node)
{
	assert(node);
	struct wlr_scene_node *prev;
	prev = wl_container_of(node->link.prev, node, link);
	if (&prev->link == &node->parent->children) {
		return NULL;
	}
	return prev;
}

static void
magnify(struct output *output, struct wlr_buffer *output_buffer)
{
	struct server *server = output->server;
	struct wlr_cursor *cursor = server->seat.cursor;
	double ox = cursor->x;
	double oy = cursor->y;
	wlr_output_layout_output_coords(server->output_layout, output->wlr_output, &ox, &oy);

	int border = 10;
	pixman_rectangle16_t crop_rect = {
		(int) ox - 100,
		(int) oy - 100,
		//output_buffer->width / 2 - 100,
		//output_buffer->height / 2 - 100,
		200, 200
	};

	wlr_buffer_lock(output_buffer);
	uint32_t flags = WLR_BUFFER_DATA_PTR_ACCESS_READ
		| WLR_BUFFER_DATA_PTR_ACCESS_WRITE;
	size_t stride;
	uint32_t drm_format;
	void *data;
	if (!wlr_buffer_begin_data_ptr_access(output_buffer, flags,
			&data, &drm_format, &stride)) {
		wlr_log(WLR_ERROR, "Failed to access data buffer");
		goto cleanup;
	}
#if 0
	/* Not public, hrm. */
	pixman_format_code_t format = get_pixman_format_from_drm(drm_format);
	if (!format) {
		wlr_log(WLR_ERROR, "Failed to parse drm_format");
		goto cleanup;
	}
#else
	pixman_format_code_t format = PIXMAN_a8r8g8b8;
#endif

	pixman_image_t *image = pixman_image_create_bits_no_clear(format,
		output_buffer->width, output_buffer->height, data, stride);
	if (!image) {
		wlr_log(WLR_ERROR, "Failed to import pixman image");
		goto cleanup;
	}

	/* Extract the area we want to magnify */
	pixman_image_t *extract = pixman_image_create_bits(format,
		crop_rect.width, crop_rect.height, NULL, 0);
	pixman_image_composite32(
		PIXMAN_OP_SRC,
		image,
		NULL,
		extract,
		crop_rect.x, crop_rect.y, /* src x,y */
		0, 0,                     /* mask x,y */
		0, 0,                     /* dst x,y */
		crop_rect.width,
		crop_rect.height
	);

	/* Set up scaling */
	pixman_transform_t transform;
	pixman_transform_init_identity(&transform);
	pixman_transform_scale(NULL, &transform,
		pixman_int_to_fixed(2), pixman_int_to_fixed(2));
	pixman_image_set_transform(extract, &transform);
	pixman_image_set_filter(extract, PIXMAN_FILTER_BEST, NULL, 0);

	/* Border */
	pixman_color_t fill_color = { 0xffff, 0, 0, 0xffff };
	pixman_image_t *fill = pixman_image_create_solid_fill(&fill_color);
	pixman_image_composite32(
		PIXMAN_OP_OVER,
		fill,
		NULL,
		image,
		0, 0, /* src x,y */
		0, 0, /* mask x,y */
		crop_rect.x - crop_rect.width / 2 - border,
		crop_rect.y - crop_rect.height / 2 - border,
		crop_rect.width * 2 + border * 2, crop_rect.height * 2 + border * 2
	);
	pixman_image_unref(fill);

	/* Blit image back to the buffer */
	pixman_image_composite32(
		PIXMAN_OP_OVER,
		extract,
		NULL,
		image,
		0, 0, /* src x,y */
		0, 0, /* mask x,y */
		crop_rect.x - crop_rect.width / 2, crop_rect.y - crop_rect.height / 2,
		crop_rect.width * 2, crop_rect.height * 2
	);
	pixman_image_unref(extract);

	//wlr_log(WLR_INFO, "Got buffer with format 0x%x and stride %lu", drm_format, stride);
	wlr_buffer_end_data_ptr_access(output_buffer);
cleanup:
	wlr_buffer_unlock(output_buffer);
}

/*
 * This is a copy of wlr_scene_output_commit()
 * as it doesn't use the pending state at all.
 */
bool
lab_wlr_scene_output_commit(struct wlr_scene_output *scene_output)
{
	assert(scene_output);
	struct wlr_output *wlr_output = scene_output->output;
	struct wlr_output_state *state = &wlr_output->pending;
	struct output *output = wlr_output->data;
	bool wants_magnification = output_nearest_to_cursor(output->server) == output;

	if (!wlr_output->needs_frame && !wants_magnification &&
			!pixman_region32_not_empty(&scene_output->damage_ring.current)) {
		return false;
	}
	if (!wlr_scene_output_build_state(scene_output, state, NULL)) {
		wlr_log(WLR_ERROR, "Failed to build output state for %s",
			wlr_output->name);
		return false;
	}

	if (state->buffer && wants_magnification) {
		magnify(output, state->buffer);
	}
	if (!wlr_output_commit(wlr_output)) {
		wlr_log(WLR_INFO, "Failed to commit output %s",
			wlr_output->name);
		return false;
	}
	/*
	 * FIXME: Remove the following line as soon as
	 * https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/4253
	 * is merged. At that point wlr_scene handles damage tracking internally
	 * again.
	 */
	wlr_damage_ring_rotate(&scene_output->damage_ring);
	return true;
}
