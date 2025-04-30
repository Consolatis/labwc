// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/graphic-helpers.h"
#include "common/macros.h"
#include "common/mem.h"
#include "common/scaled-scene-buffer.h"
#include "common/scaled-titlebar-buffer.h"
#include "node.h"

/* 1 degree in radians (=2π/360) */
static const double deg = 0.017453292519943295;

static void
draw_titlebar_outline(cairo_t *cairo, double w, double h, double r, double delta)
{
	cairo_move_to(cairo, delta, h);
	cairo_line_to(cairo, delta, r);
	cairo_arc(cairo, r, r, r - delta, 180 * deg, 270 * deg);
	cairo_line_to(cairo, w - r, delta);
	cairo_arc(cairo, w - r, r, r - delta, 270 *deg, 360 * deg);
	cairo_line_to(cairo, w - delta, h);
}

static void
set_pattern_range(cairo_pattern_t *pattern,
		double x, double y, double w, double h)
{
	w = MAX(1, w);
	h = MAX(1, h);

	cairo_matrix_t matrix;
	cairo_matrix_init_scale(&matrix, 1.0 / w, 1.0 / h);
	cairo_matrix_translate(&matrix, -x, -y);
	cairo_pattern_set_matrix(pattern, &matrix);
}

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
	struct scaled_titlebar_buffer *self = scaled_buffer->data;
	struct lab_data_buffer *buffer = buffer_create_cairo(
		self->width, self->height, scale);

	if (!buffer) {
		return NULL;
	}

	int radius = (self->corner_radius * 2 < self->width && !self->squared_corners) ?
		self->corner_radius : 0;

	wlr_log(WLR_INFO, "creating new titlebar buffer with corner radius %d", radius);

	cairo_t *cairo = cairo_create(buffer->surface);

	/* Clear background */
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);

	/* Draw background */
	draw_titlebar_outline(cairo, self->width, self->height, radius,
		self->border_width);
	set_pattern_range(self->fill_pattern,
		self->border_width, self->border_width,
		self->width - self->border_width * 2,
		self->height - self->border_width * 2);
	cairo_set_source(cairo, self->fill_pattern);
	cairo_set_line_width(cairo, 0.0);
	cairo_fill(cairo);
	set_pattern_range(self->fill_pattern, 0, 0, 1, 1);

	/* Draw border */
	draw_titlebar_outline(cairo, self->width, self->height, radius,
		self->border_width / 2.0);
	set_cairo_color(cairo, self->border_color);
	cairo_set_line_width(cairo, self->border_width);
	cairo_stroke(cairo);

	cairo_surface_flush(buffer->surface);
	cairo_destroy(cairo);

	return buffer;
}

static void
_destroy(struct scaled_scene_buffer *scaled_buffer)
{
	struct scaled_titlebar_buffer *self = scaled_buffer->data;
	cairo_pattern_destroy(self->fill_pattern);
	scaled_buffer->data = NULL;
	free(self);
}

static bool
_equal(struct scaled_scene_buffer *scaled_buffer_a, struct scaled_scene_buffer *scaled_buffer_b)
{
	struct scaled_titlebar_buffer *a = scaled_buffer_a->data;
	struct scaled_titlebar_buffer *b = scaled_buffer_b->data;

	return a->width == b->width
		&& a->height == b->height
		&& a->border_width == b->border_width
		&& a->corner_radius == b->corner_radius
		&& a->squared_corners == b->squared_corners
		&& a->fill_pattern == b->fill_pattern
		&& !memcmp(a->border_color, b->border_color, sizeof(a->border_color));
}

static const struct scaled_scene_buffer_impl impl = {
	.create_buffer = _create_buffer,
	.destroy = _destroy,
	.equal = _equal,
};

struct scaled_titlebar_buffer *
scaled_titlebar_buffer_create(struct wlr_scene_tree *parent, int width,
		int height, int border_width, int corner_radius,
		cairo_pattern_t *fill_pattern, float border_color[4])
{
	assert(parent);
	struct scaled_titlebar_buffer *self = znew(*self);
	struct scaled_scene_buffer *scaled_buffer = scaled_scene_buffer_create(
		parent, &impl, /* drop_buffer */ true);
	scaled_buffer->data = self;
	self->scaled_buffer = scaled_buffer;
	self->scene_buffer = scaled_buffer->scene_buffer;

	self->width = MAX(width, 1);
	self->height = MAX(height, 1);
	self->border_width = MAX(border_width, 0);
	self->corner_radius = MAX(0, corner_radius);
	cairo_pattern_reference(fill_pattern);
	self->fill_pattern = fill_pattern;
	memcpy(self->border_color, border_color, sizeof(self->border_color));

	scaled_scene_buffer_request_update(self->scaled_buffer,
		self->width, self->height);

	return self;
}

void
scaled_titlebar_buffer_set_size(struct scaled_titlebar_buffer *self,
		int width, int height)
{
	self->width = width;
	self->height = height;
	scaled_scene_buffer_request_update(self->scaled_buffer,
		self->width, self->height);
}

void
scaled_titlebar_buffer_set_square(struct scaled_titlebar_buffer *self, bool enabled)
{
	self->squared_corners = enabled;
	scaled_scene_buffer_request_update(self->scaled_buffer,
		self->width, self->height);
}

struct scaled_titlebar_buffer *
scaled_titlebar_buffer_from_node(struct wlr_scene_node *node)
{
	struct scaled_scene_buffer *scaled_buffer =
		node_scaled_scene_buffer_from_node(node);
	assert(scaled_buffer->impl == &impl);
	return scaled_buffer->data;
}
