// SPDX-License-Identifier: GPL-2.0-only

#include "render.h"
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/mem.h"


static uint32_t*
create_corner_square(uint32_t *pixels, size_t pixel_width)
{
	uint32_t *corner = xmalloc(pixel_width * pixel_width * 4);
	for (size_t y = 0; y < pixel_width; y++) {
		for (size_t x = 0; x < pixel_width; x++) {
			corner[y * pixel_width + x] = pixels[x > y ? y : x];
		}
	}
	return corner;
}

#include <math.h>
#include <wlr/util/log.h>

static uint32_t *
create_corner_round(uint32_t *pixels, size_t pixel_width)
{
	const size_t max_index = pixel_width - 1;

	/*
	 * This is the top-left corner of the rect.
	 * Thus we are taking the bottom right pixel of this corner as reference point.
	 * Maximal distance is from the bottom right pixel to the bottom left one, e.g.
	 * the bottom edge. Usually we would thus calculate the euclidean distance via
	 * sqrt((x1 - x2)**2 + (y1 - y2)**2).
	 * As this whole thing is a square the distance is the same for the upper edge.
	 * Calculating that distance would thus be sqrt((max_index - 0)**2 + (0 - 0)**2)
	 * which can be simplified to sqrt((max_index - 0)**2) and thus simply max_index.
	 */
	double max_dist = max_index;

	// TODO: maybe blend pixels near the outside by fraction of index_dist_diff
	uint32_t *corner = xmalloc(pixel_width * pixel_width * 4);
	for (size_t y = 0; y < pixel_width; y++) {
		for (size_t x = 0; x < pixel_width; x++) {
			/* Get the distance to the bottom right pixel */
			double dx = max_index - x;
			double dy = max_index - y;
			double distance = sqrt(dx * dx + dy * dy);

			/* Compare it to the max distance and map it to our pattern */
			double dist_fac = distance / max_dist;
			size_t index = max_index - (size_t)(dist_fac * max_index + 0.5f);

			if (index > max_index) {
				/* Outside of our rounded rect, set it transparent */
				corner[y * pixel_width + x] = 0x00000000u;
			} else {
				corner[y * pixel_width + x] = pixels[index];
			}
		}
	}
	return corner;
}

struct textured_rect_buffer *
textured_rect_buffer_from_pixels(uint32_t *pixels, size_t pixel_width, uint32_t style)
{
	struct textured_rect_buffer *buf = znew(*buf);
	buf->edge = buffer_create_from_data(pixels, pixel_width, 1, pixel_width * 4);

	if (style == LAB_TEX_RECT_BUFFER_SQUARE) {
		uint32_t *corner = create_corner_square(pixels, pixel_width);
		buf->tl = buffer_create_from_data(corner, pixel_width, pixel_width, pixel_width * 4);
		buf->tr = buf->bl = buf->br = buf->tl;
	} else if (style == LAB_TEX_RECT_BUFFER_ROUND) {
		uint32_t *corner = create_corner_round(pixels, pixel_width);
		buf->tl = buffer_create_from_data(corner, pixel_width, pixel_width, pixel_width * 4);
		buf->tr = buf->bl = buf->br = buf->tl;
	} else {
		uint32_t *corner_square = create_corner_square(pixels, pixel_width);
		struct lab_data_buffer *square = buffer_create_from_data(
			corner_square, pixel_width, pixel_width, pixel_width * 4);

		uint32_t *corner_round = create_corner_round(pixels, pixel_width);
		struct lab_data_buffer *round = buffer_create_from_data(
			corner_round, pixel_width, pixel_width, pixel_width * 4);

		buf->tl = (style & LAB_TEX_RECT_BUFFER_ROUND_TL) ? round : square;
		buf->tr = (style & LAB_TEX_RECT_BUFFER_ROUND_TR) ? round : square;
		buf->bl = (style & LAB_TEX_RECT_BUFFER_ROUND_BL) ? round : square;
		buf->br = (style & LAB_TEX_RECT_BUFFER_ROUND_BR) ? round : square;
	}

	return buf;
}

static uint8_t
_add_buffer(struct wlr_buffer *buffers[], struct wlr_buffer *buffer, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		if (buffers[i] == buffer) {
			return 0;
		}
	}
	buffers[count] = buffer;
	return 1;
}

void
textured_rect_buffer_destroy(struct textured_rect_buffer *textured_rect_buf)
{
	wlr_buffer_drop(&textured_rect_buf->edge->base);

	/* The corner buffers may be re-used thus we have to ensure to drop them only once */
	struct wlr_buffer *buffers[4];
	size_t count = 0;
	count += _add_buffer(buffers, &textured_rect_buf->tl->base, count);
	count += _add_buffer(buffers, &textured_rect_buf->tr->base, count);
	count += _add_buffer(buffers, &textured_rect_buf->bl->base, count);
	count += _add_buffer(buffers, &textured_rect_buf->br->base, count);
	for (size_t i = 0; i < count; i++) {
		wlr_buffer_drop(buffers[i]);
	}

	free(textured_rect_buf);
}

static void
rect_handle_tree_destroy(struct wl_listener *listener, void *data)
{
	struct textured_rect *rect = wl_container_of(listener, rect, on.tree_destroy);
	wl_list_remove(&rect->on.tree_destroy.link);
	free(rect);
}

struct textured_rect *
textured_rect_create(struct wlr_scene_tree *parent,
		struct textured_rect_buffer *buf,
		uint32_t thickness, uint32_t width, uint32_t height)
{
	struct textured_rect *rect = znew(*rect);
	rect->tree = wlr_scene_tree_create(parent);

	for (size_t i = LAB_TEX_RECT_TOP; i < LAB_TEX_RECT_TL; i++) {
		rect->buffers[i] = wlr_scene_buffer_create(rect->tree, &buf->edge->base);
		//rect->buffers[i]->filter_mode = WLR_SCALE_FILTER_NEAREST;
	}

	rect->buffers[LAB_TEX_RECT_TL] = wlr_scene_buffer_create(rect->tree, &buf->tl->base);
	rect->buffers[LAB_TEX_RECT_TR] = wlr_scene_buffer_create(rect->tree, &buf->tr->base);
	rect->buffers[LAB_TEX_RECT_BL] = wlr_scene_buffer_create(rect->tree, &buf->bl->base);
	rect->buffers[LAB_TEX_RECT_BR] = wlr_scene_buffer_create(rect->tree, &buf->br->base);
	for (size_t i = LAB_TEX_RECT_TL; i < LAB_TEX_RECT_COUNT; i++) {
		//rect->buffers[i]->filter_mode = WLR_SCALE_FILTER_NEAREST;
	}

	wlr_scene_buffer_set_transform(rect->buffers[LAB_TEX_RECT_TOP], WL_OUTPUT_TRANSFORM_90);
	wlr_scene_buffer_set_transform(rect->buffers[LAB_TEX_RECT_RIGHT], WL_OUTPUT_TRANSFORM_180);
	wlr_scene_buffer_set_transform(rect->buffers[LAB_TEX_RECT_BOTTOM], WL_OUTPUT_TRANSFORM_270);
	wlr_scene_buffer_set_transform(rect->buffers[LAB_TEX_RECT_TR], WL_OUTPUT_TRANSFORM_90);
	wlr_scene_buffer_set_transform(rect->buffers[LAB_TEX_RECT_BR], WL_OUTPUT_TRANSFORM_180);
	wlr_scene_buffer_set_transform(rect->buffers[LAB_TEX_RECT_BL], WL_OUTPUT_TRANSFORM_270);

	textured_rect_set_size(rect, thickness, width, height);

#if 0
	// debug only
	for (size_t i = 0; i < LAB_TEX_RECT_COUNT; i++) {
		wlr_scene_node_set_enabled(&rect->buffers[i]->node, false);
	}
	wlr_scene_node_set_enabled(&rect->buffers[LAB_TEX_RECT_LEFT]->node, true);
#endif
	rect->on.tree_destroy.notify = rect_handle_tree_destroy;
	wl_signal_add(&rect->tree->node.events.destroy, &rect->on.tree_destroy);

	return rect;
}

void
textured_rect_set_size(struct textured_rect *rect,
	uint32_t thickness, uint32_t width, uint32_t height)
{
	for (size_t i = LAB_TEX_RECT_TOP; i <= LAB_TEX_RECT_BOTTOM; i++) {
		wlr_scene_buffer_set_dest_size(rect->buffers[i], width - 2 * thickness, thickness);
	}
	for (size_t i = LAB_TEX_RECT_LEFT; i <= LAB_TEX_RECT_RIGHT; i++) {
		wlr_scene_buffer_set_dest_size(rect->buffers[i], thickness, height - 2 * thickness);
	}
	for (size_t i = LAB_TEX_RECT_TL; i < LAB_TEX_RECT_COUNT; i++) {
		wlr_scene_buffer_set_dest_size(rect->buffers[i], thickness, thickness);
	}

	wlr_scene_node_set_position(&rect->buffers[LAB_TEX_RECT_TOP]->node, thickness, 0);
	wlr_scene_node_set_position(
		&rect->buffers[LAB_TEX_RECT_BOTTOM]->node, thickness, height - thickness);
	wlr_scene_node_set_position(&rect->buffers[LAB_TEX_RECT_LEFT]->node, 0, thickness);
	wlr_scene_node_set_position(
		&rect->buffers[LAB_TEX_RECT_RIGHT]->node, width - thickness, thickness);

	wlr_scene_node_set_position(&rect->buffers[LAB_TEX_RECT_TR]->node, width - thickness, 0);
	wlr_scene_node_set_position(
		&rect->buffers[LAB_TEX_RECT_BR]->node, width - thickness, height - thickness);
	wlr_scene_node_set_position(&rect->buffers[LAB_TEX_RECT_BL]->node, 0, height - thickness);
}
