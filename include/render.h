/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_RENDER_H
#define LABWC_RENDER_H

#include <stddef.h>
#include <stdint.h>
#include <wayland-server-core.h>

enum textured_rect_buffer_style {
	LAB_TEX_RECT_BUFFER_SQUARE = 0,
	LAB_TEX_RECT_BUFFER_ROUND_TL = 1 << 0,
	LAB_TEX_RECT_BUFFER_ROUND_TR = 1 << 1,
	LAB_TEX_RECT_BUFFER_ROUND_BL = 1 << 2,
	LAB_TEX_RECT_BUFFER_ROUND_BR = 1 << 3,

	LAB_TEX_RECT_BUFFER_ROUND_TOP =
		LAB_TEX_RECT_BUFFER_ROUND_TL | LAB_TEX_RECT_BUFFER_ROUND_TR,

	LAB_TEX_RECT_BUFFER_ROUND_BOTTOM =
		LAB_TEX_RECT_BUFFER_ROUND_BL | LAB_TEX_RECT_BUFFER_ROUND_BR,

	LAB_TEX_RECT_BUFFER_ROUND_LEFT =
		LAB_TEX_RECT_BUFFER_ROUND_TL | LAB_TEX_RECT_BUFFER_ROUND_BL,

	LAB_TEX_RECT_BUFFER_ROUND_RIGHT =
		LAB_TEX_RECT_BUFFER_ROUND_TR | LAB_TEX_RECT_BUFFER_ROUND_BR,

	LAB_TEX_RECT_BUFFER_ROUND =
		LAB_TEX_RECT_BUFFER_ROUND_TOP | LAB_TEX_RECT_BUFFER_ROUND_BOTTOM,
};

struct textured_rect_buffer {
	struct lab_data_buffer *edge;
	struct lab_data_buffer *tl;
	struct lab_data_buffer *tr;
	struct lab_data_buffer *bl;
	struct lab_data_buffer *br;
};

/* Create a textured rect buffer based on a 1xN pixel pattern, takes ownership of the pixels */
struct textured_rect_buffer *textured_rect_buffer_from_pixels(uint32_t *pixels, size_t pixel_width, uint32_t style);

/* Destroy the rect buffer and drop the internal lab_data_buffers, scene_trees may outlive the rect */
void textured_rect_buffer_destroy(struct textured_rect_buffer *textured_rect_buf);

enum textured_rect_scene_buffers {
	LAB_TEX_RECT_TOP = 0,
	LAB_TEX_RECT_BOTTOM,
	LAB_TEX_RECT_LEFT,
	LAB_TEX_RECT_RIGHT,
	LAB_TEX_RECT_TL,
	LAB_TEX_RECT_TR,
	LAB_TEX_RECT_BL,
	LAB_TEX_RECT_BR,

	LAB_TEX_RECT_COUNT
};

struct textured_rect {
	struct wlr_scene_tree *tree;
	struct wlr_scene_buffer *buffers[LAB_TEX_RECT_COUNT];

	struct {
		struct wl_listener tree_destroy;
	} on;
};

/*
 * Create a scene tree based on a textured rect buffer with given dimensions
 *
 * Frees itself if the textured_rect->tree is destroyed.
 */
struct textured_rect *textured_rect_create(struct wlr_scene_tree *parent,
	struct textured_rect_buffer *textured_rect_buf,
	uint32_t thickness, uint32_t width, uint32_t height);

void textured_rect_set_size(struct textured_rect *rect, uint32_t thickness, uint32_t width, uint32_t height);

void render_jl(uint32_t pixels[], uint32_t bg_argb, uint32_t width);


#endif /* LABWC_RENDER_H */
