// SPDX-License-Identifier: GPL-2.0-only

#include "render.h"
#include <wlr/types/wlr_scene.h>
#include "buffer.h"
#include "common/mem.h"

enum _TEXTURED_REC {
	TEX_RECT_TOP = 0,
	TEX_RECT_LEFT,
	TEX_RECT_RIGHT,
	TEX_RECT_BOTTOM,
	TEX_RECT_TL,
	TEX_RECT_TR,
	TEX_RECT_BL,
	TEX_RECT_BR,

	TEX_RECT_SIZE
};

struct textured_rect *
textured_rect_create_from_pixels(uint32_t *pixels, size_t pixel_width)
{
	uint32_t *corner = xmalloc(pixel_width * pixel_width * 4);
	for (size_t y = 0; y < pixel_width; y++) {
		for (size_t x = 0; x < pixel_width; x++) {
			corner[y * pixel_width + x] = pixels[x > y ? y : x];
		}
	}

	struct textured_rect *textured_rect = znew(*textured_rect);
	textured_rect->edge = buffer_create_from_data(pixels, pixel_width, 1, pixel_width * 4);
	textured_rect->corner = buffer_create_from_data(corner, pixel_width, pixel_width, pixel_width * 4);
	return textured_rect;
}

struct wlr_scene_tree *
textured_rect_create_tree(struct wlr_scene_tree *parent, struct textured_rect *textured_rect,
		uint32_t thickness, uint32_t width, uint32_t height)
{
	struct wlr_scene_tree *tree = wlr_scene_tree_create(parent);
	struct wlr_scene_buffer *buffers[TEX_RECT_SIZE];

	for (size_t i = 0; i < TEX_RECT_SIZE; i++) {
		struct wlr_buffer *buffer = i < TEX_RECT_TL
			? &textured_rect->edge->base
			: &textured_rect->corner->base;
		buffers[i] = wlr_scene_buffer_create(tree, buffer);
		buffers[i]->filter_mode = WLR_SCALE_FILTER_NEAREST;
	}

	wlr_scene_buffer_set_transform(buffers[TEX_RECT_TOP], WL_OUTPUT_TRANSFORM_90);
	wlr_scene_buffer_set_transform(buffers[TEX_RECT_RIGHT], WL_OUTPUT_TRANSFORM_180);
	wlr_scene_buffer_set_transform(buffers[TEX_RECT_BOTTOM], WL_OUTPUT_TRANSFORM_270);
	wlr_scene_buffer_set_transform(buffers[TEX_RECT_TR], WL_OUTPUT_TRANSFORM_90);
	wlr_scene_buffer_set_transform(buffers[TEX_RECT_BR], WL_OUTPUT_TRANSFORM_180);
	wlr_scene_buffer_set_transform(buffers[TEX_RECT_BL], WL_OUTPUT_TRANSFORM_270);

	wlr_scene_buffer_set_dest_size(buffers[TEX_RECT_LEFT], thickness, height - 2 * thickness);
	wlr_scene_buffer_set_dest_size(buffers[TEX_RECT_RIGHT], thickness, height - 2 * thickness);
	wlr_scene_buffer_set_dest_size(buffers[TEX_RECT_TOP], width - 2 * thickness, thickness);
	wlr_scene_buffer_set_dest_size(buffers[TEX_RECT_BOTTOM], width - 2 * thickness, thickness);
	for (size_t i = TEX_RECT_TL; i < TEX_RECT_SIZE; i++) {
		wlr_scene_buffer_set_dest_size(buffers[i], thickness, thickness);
	}

	wlr_scene_node_set_position(&buffers[TEX_RECT_TOP]->node, thickness, 0);
	wlr_scene_node_set_position(&buffers[TEX_RECT_BOTTOM]->node, thickness, height - thickness);
	wlr_scene_node_set_position(&buffers[TEX_RECT_LEFT]->node, 0, thickness);
	wlr_scene_node_set_position(&buffers[TEX_RECT_RIGHT]->node, width - thickness, thickness);

	wlr_scene_node_set_position(&buffers[TEX_RECT_TR]->node, width - thickness, 0);
	wlr_scene_node_set_position(&buffers[TEX_RECT_BR]->node, width - thickness, height - thickness);
	wlr_scene_node_set_position(&buffers[TEX_RECT_BL]->node, 0, height - thickness);

	return tree;
}

void
textured_rect_destroy(struct textured_rect *textured_rect)
{
	wlr_buffer_drop(&textured_rect->edge->base);
	wlr_buffer_drop(&textured_rect->corner->base);
	free(textured_rect);
}
