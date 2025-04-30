/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCALED_RECT_BUFFER_H
#define LABWC_SCALED_RECT_BUFFER_H

#include <cairo.h>
#include <stdint.h>

struct wlr_scene_tree;
struct wlr_scene_node;
struct wlr_scene_buffer;
struct scaled_scene_buffer;

struct scaled_titlebar_buffer_rects {
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *left;
	struct wlr_scene_rect *right;
	struct wlr_scene_rect *top;
	struct wlr_scene_rect *fill;
};

struct scaled_titlebar_buffer {
	struct wlr_scene_buffer *scene_buffer;
	struct scaled_scene_buffer *scaled_buffer;

	int width;
	int height;
	int border_width;
	int corner_radius;
	bool squared_corners;
	cairo_pattern_t *fill_pattern;
	float border_color[4];
};

/*
 * TODO: document
 */
struct scaled_titlebar_buffer *scaled_titlebar_buffer_create(
	struct wlr_scene_tree *parent, int width, int height, int border_width,
	int corner_radius, cairo_pattern_t *fill_pattern, float border_color[4]);

void scaled_titlebar_buffer_set_size(struct scaled_titlebar_buffer *self,
	int width, int height);

void scaled_titlebar_buffer_set_square(struct scaled_titlebar_buffer *self, bool enabled);

struct scaled_titlebar_buffer *scaled_titlebar_buffer_from_node(
	struct wlr_scene_node *node);

#endif /* LABWC_SCALED_RECT_BUFFER_H */
