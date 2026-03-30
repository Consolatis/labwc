/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_RENDER_H
#define LABWC_RENDER_H

#include <stddef.h>
#include <stdint.h>

struct textured_rect {
	struct lab_data_buffer *edge;
	struct lab_data_buffer *corner;
};

/* Create a textured rect based on a 1xN pixel pattern, takes ownership of the pixels */
struct textured_rect *textured_rect_create_from_pixels(uint32_t *pixels, size_t pixel_width);

/* Create a scene tree based on a textured rect with given dimensions */
struct wlr_scene_tree *textured_rect_create_tree(struct wlr_scene_tree *parent,
	struct textured_rect *textured_rect, uint32_t thickness, uint32_t width, uint32_t height);

/* Destroy the rect and drop the internal lab_data_buffers, scene_trees may outlive the rect */
void textured_rect_destroy(struct textured_rect *textured_rect);

#endif /* LABWC_RENDER_H */
