// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "common/array-size.h"
#include "common/graphic-helpers.h"
#include "common/mem.h"
#include "common/scaled_scene_buffer.h"
#include "config/rcxml.h"
#include "ssd.h"
#include "theme.h"

// FIXME: remove and include labwc.h
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

// FIXME: debug the lab_data_buffer destroy signal to ensure we don't leak or UAF



/* Used as argument for rounded_rect_create() */
struct rounded_corner_ctx {
	struct wlr_box *box;
	double radius;
	double line_width;
	float *fill_color;
	float *border_color;
	enum lab_corner corner;
};

struct cache_entry {
	double scale;
	enum lab_corner corner;
	struct lab_data_buffer *buffer;
};

static bool corners_initialized = false;

/* Local buffer cache */
static struct wl_array buffer_cache;


static struct lab_data_buffer *
rounded_rect_create(struct rounded_corner_ctx *ctx, double scale)
{
	/* 1 degree in radians (=2π/360) */
	double deg = 0.017453292519943295;

	double w = ctx->box->width;
	double h = ctx->box->height;
	double r = ctx->radius;

	struct lab_data_buffer *buffer =
		buffer_create_cairo(w, h, scale, /* free_on_destroy */ true);

	cairo_t *cairo = buffer->cairo;
	cairo_surface_t *surf = cairo_get_target(cairo);

	/* set transparent background */
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);

	/*
	 * Create outline path and fill. Illustration of top-left corner buffer:
	 *
	 *          _,,ooO"""""""""+
	 *        ,oO"'   ^        |
	 *      ,o"       |        |
	 *     o"         |r       |
	 *    o'          |        |
	 *    O     r     v        |
	 *    O<--------->+        |
	 *    O                    |
	 *    O                    |
	 *    O                    |
	 *    +--------------------+
	 */
	cairo_set_line_width(cairo, 0.0);
	cairo_new_sub_path(cairo);
	switch (ctx->corner) {
	case LAB_CORNER_TOP_LEFT_ACTIVE:
	case LAB_CORNER_TOP_LEFT_INACTIVE:
		cairo_arc(cairo, r, r, r, 180 * deg, 270 * deg);
		cairo_line_to(cairo, w, 0);
		cairo_line_to(cairo, w, h);
		cairo_line_to(cairo, 0, h);
		break;
	case LAB_CORNER_TOP_RIGHT_ACTIVE:
	case LAB_CORNER_TOP_RIGHT_INACTIVE:
		cairo_arc(cairo, w - r, r, r, -90 * deg, 0 * deg);
		cairo_line_to(cairo, w, h);
		cairo_line_to(cairo, 0, h);
		cairo_line_to(cairo, 0, 0);
		break;
	default:
		wlr_log(WLR_ERROR, "unknown corner type");
	}
	cairo_close_path(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	set_cairo_color(cairo, ctx->fill_color);
	cairo_fill_preserve(cairo);
	cairo_stroke(cairo);

	/*
	 * Stroke horizontal and vertical borders, shown by Xs and Ys
	 * respectively in the figure below:
	 *
	 *          _,,ooO"XXXXXXXXX
	 *        ,oO"'            |
	 *      ,o"                |
	 *     o"                  |
	 *    o'                   |
	 *    O                    |
	 *    Y                    |
	 *    Y                    |
	 *    Y                    |
	 *    Y                    |
	 *    Y--------------------+
	 */
	cairo_set_line_cap(cairo, CAIRO_LINE_CAP_BUTT);
	set_cairo_color(cairo, ctx->border_color);
	cairo_set_line_width(cairo, ctx->line_width);
	double half_line_width = ctx->line_width / 2.0;
	switch (ctx->corner) {
	case LAB_CORNER_TOP_LEFT_ACTIVE:
	case LAB_CORNER_TOP_LEFT_INACTIVE:
		cairo_move_to(cairo, half_line_width, h);
		cairo_line_to(cairo, half_line_width, r);
		cairo_move_to(cairo, r, half_line_width);
		cairo_line_to(cairo, w, half_line_width);
		break;
	case LAB_CORNER_TOP_RIGHT_ACTIVE:
	case LAB_CORNER_TOP_RIGHT_INACTIVE:
		cairo_move_to(cairo, 0, half_line_width);
		cairo_line_to(cairo, w - r, half_line_width);
		cairo_move_to(cairo, w - half_line_width, r);
		cairo_line_to(cairo, w - half_line_width, h);
		break;
	default:
		wlr_log(WLR_ERROR, "unknown corner type");
	}
	cairo_stroke(cairo);

	/*
	 * If radius==0 the borders stroked above go right up to (and including)
	 * the corners, so there is not need to do any more.
	 */
	if (!r) {
		goto out;
	}

	/*
	 * Stroke the arc section of the border of the corner piece.
	 *
	 * Note: This figure is drawn at a more zoomed in scale compared with
	 * those above.
	 *
	 *                 ,,ooooO""  ^
	 *            ,ooo""'      |  |
	 *         ,oOO"           |  | line-thickness
	 *       ,OO"              |  |
	 *     ,OO"         _,,ooO""  v
	 *    ,O"         ,oO"'
	 *   ,O'        ,o"
	 *  ,O'        o"
	 *  o'        o'
	 *  O         O
	 *  O---------O            +
	 *       <----------------->
	 *          radius
	 *
	 * We handle the edge-case where line-thickness > radius by merely
	 * setting line-thickness = radius and in effect drawing a quadrant of a
	 * circle. In this case the X and Y borders butt up against the arc and
	 * overlap each other (as their line-thickessnes are greater than the
	 * linethickness of the arc). As a result, there is no inner rounded
	 * corners.
	 *
	 * So, in order to have inner rounded corners cornerRadius should be
	 * greater than border.width.
	 *
	 * Also, see diagrams in https://github.com/labwc/labwc/pull/990
	 */
	double line_width = MIN(ctx->line_width, r);
	cairo_set_line_width(cairo, line_width);
	half_line_width = line_width / 2.0;
	switch (ctx->corner) {
	case LAB_CORNER_TOP_LEFT_ACTIVE:
	case LAB_CORNER_TOP_LEFT_INACTIVE:
		cairo_move_to(cairo, half_line_width, r);
		cairo_arc(cairo, r, r, r - half_line_width, 180 * deg, 270 * deg);
		break;
	case LAB_CORNER_TOP_RIGHT_ACTIVE:
	case LAB_CORNER_TOP_RIGHT_INACTIVE:
		cairo_move_to(cairo, w - r, half_line_width);
		cairo_arc(cairo, w - r, r, r - half_line_width, -90 * deg, 0 * deg);
		break;
	default:
		break;
	}
	cairo_stroke(cairo);

out:
	cairo_surface_flush(surf);

	return buffer;
}


static struct lab_data_buffer *
create_corner(struct theme *theme, enum lab_corner corner, double scale)
{

	struct wlr_box box = {
		.x = 0,
		.y = 0,
		.width = SSD_BUTTON_WIDTH + theme->border_width,
		.height = theme->title_height + theme->border_width,
	};

	struct rounded_corner_ctx ctx = {
		.box = &box,
		.radius = rc.corner_radius,
		.line_width = theme->border_width,
		.corner = corner
	};
	switch(corner) {
	case LAB_CORNER_TOP_LEFT_ACTIVE:
	case LAB_CORNER_TOP_RIGHT_ACTIVE:
		ctx.fill_color = theme->window_active_title_bg_color;
		ctx.border_color = theme->window_active_border_color;
		break;
	case LAB_CORNER_TOP_LEFT_INACTIVE:
	case LAB_CORNER_TOP_RIGHT_INACTIVE:
		ctx.fill_color = theme->window_inactive_title_bg_color;
		ctx.border_color = theme->window_inactive_border_color;
		break;
	}
	return rounded_rect_create(&ctx, scale);
}

static struct lab_data_buffer *
corners_get_buffer(struct theme *theme, enum lab_corner corner, double scale)
{
	struct cache_entry *entry;

	/* Check for existing buffer */
	wl_array_for_each(entry, &buffer_cache) {
		if (entry->corner == corner && entry->scale == scale) {
			wlr_log(WLR_INFO, "Returning cached corner buffer "
				"for corner %u and scale %.2f", corner, scale);
			return entry->buffer;
		}
	}

	wlr_log(WLR_INFO, "Creating new corner buffer for corner %u and scale %.2f", corner, scale);

	/*
	 * Create new one and store within the cache
	 * (not just the pointer but the whole struct)
	 */
	entry = wl_array_add(&buffer_cache, sizeof(*entry));
	entry->scale = scale;
	entry->corner = corner;
	entry->buffer = create_corner(theme, corner, scale);

	/* Prevent dropping the buffer */
	wlr_log(WLR_INFO, "[%p] locking, buffer dropped: %s, locks: %lu",
		&entry->buffer->base, entry->buffer->base.dropped ? "YES" : "no",
		entry->buffer->base.n_locks);
	wlr_buffer_lock(&entry->buffer->base);
	wlr_log(WLR_INFO, "[%p] locked, buffer dropped: %s, locks: %lu",
		&entry->buffer->base, entry->buffer->base.dropped ? "YES" : "no",
		entry->buffer->base.n_locks);
	return entry->buffer;
}

static void
corners_clear_cache(void)
{
	wlr_log(WLR_INFO, "Clearing corner buffer cache");

	struct cache_entry *entry;

	/* Drop buffers */
	wl_array_for_each(entry, &buffer_cache) {
		assert(entry->buffer);
		//scaled_scene_buffer_invalidate_cache(entry->scaled_buffer);
		//wlr_log(WLR_INFO, "[%p] unlocking, buffer dropped: %s, locks: %lu",
		//	&entry->buffer->base, entry->buffer->base.dropped ? "YES" : "no",
		//	entry->buffer->base.n_locks);
		wlr_buffer_unlock(&entry->buffer->base);
		//wlr_log(WLR_INFO, "[%p] dropping, buffer dropped: %s, locks: %lu",
		//	&entry->buffer->base, entry->buffer->base.dropped ? "YES" : "no",
		//	entry->buffer->base.n_locks);
		wlr_buffer_drop(&entry->buffer->base);
		wlr_log(WLR_INFO, "[%p] dropped, buffer dropped: %s, locks: %lu",
			&entry->buffer->base, entry->buffer->base.dropped ? "YES" : "no",
			entry->buffer->base.n_locks);
		entry->buffer = NULL;
	}

	wlr_log(WLR_INFO, "Resetting corner buffer cache");

	/* Reset the array */
	wlr_log(WLR_ERROR, "before release ->data: %p", buffer_cache.data);
	wl_array_release(&buffer_cache);
	//wlr_log(WLR_ERROR, "->data: %p", buffer_cache.data);
	buffer_cache.data = NULL;
	wl_array_init(&buffer_cache);
	wlr_log(WLR_ERROR, "after reset ->data: %p", buffer_cache.data);
}

void
corners_reconfigure(struct theme *theme)
{
	if (!corners_initialized) {
		wl_array_init(&buffer_cache);
		corners_initialized = true;
	}

	corners_clear_cache();

	wlr_log(WLR_INFO, "Priming corner buffer cache for scale 1");
	/* Ensure we always have the buffers for scale 1 available */
	corners_get_buffer(theme, LAB_CORNER_TOP_LEFT_ACTIVE, 1);
	corners_get_buffer(theme, LAB_CORNER_TOP_RIGHT_ACTIVE, 1);
	corners_get_buffer(theme, LAB_CORNER_TOP_LEFT_INACTIVE, 1);
	corners_get_buffer(theme, LAB_CORNER_TOP_RIGHT_INACTIVE, 1);
}

void
corners_finish(struct theme *theme)
{
	corners_clear_cache();
	// FIXME: this is basically a copy of corners_reconfigure(). hrm.
	// FIXME: and from where is this called anyway. hrm. hrm. hrm.
	//wl_array_release(&buffer_cache);
}

static struct lab_data_buffer *
_create_buffer(struct scaled_scene_buffer *scaled_buffer, double scale)
{
	wlr_log(WLR_INFO, "corner buffer scene node for scale %.2f requested", scale);
	struct scaled_corner_buffer *self = scaled_buffer->data;
	//return corners_get_buffer(rc.theme, self->corner, scale);
	struct lab_data_buffer *buffer = corners_get_buffer(rc.theme, self->corner, scale);
	wlr_log(WLR_INFO, "[%p] created, buffer dropped: %s, locks: %lu",
		&buffer->base, buffer->base.dropped ? "YES" : "no",
		buffer->base.n_locks);
	return buffer;
}

static void
_destroy(struct scaled_scene_buffer *scaled_buffer)
{
	wlr_log(WLR_INFO, "corner buffer scene node destroyed");
	//struct scaled_font_buffer *self = scaled_buffer->data;
	zfree(scaled_buffer->data);
}

static const struct scaled_scene_buffer_impl impl = {
	.create_buffer = _create_buffer,
	.destroy = _destroy
};

/* Public API */
struct scaled_corner_buffer *
scaled_corner_buffer_create(struct wlr_scene_tree *parent, enum lab_corner corner)
{
	wlr_log(WLR_INFO, "Creating scaled corner buffer scene node");
	assert(parent);
	struct scaled_corner_buffer *self = znew(*self);
	struct scaled_scene_buffer *scaled_buffer =
		scaled_scene_buffer_create(parent, &impl, /* drop_buffer */ false);
	assert(scaled_buffer);
	if (!scaled_buffer) {
		free(self);
		return NULL;
	}

	scaled_buffer->data = self;
	//self->scaled_buffer = scaled_buffer;
	self->scene_buffer = scaled_buffer->scene_buffer;
	self->corner = corner;
	/* Invalidate cache and force a new render */
	scaled_scene_buffer_invalidate_cache(scaled_buffer);
	return self;
}
