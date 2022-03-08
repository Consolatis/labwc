// SPDX-License-Identifier: GPL-2.0-only

#include <assert.h>
#include "labwc.h"
#include "ssd.h"
#include "theme.h"
#include "common/scene-helpers.h"

void
ssd_extents_create(struct view *view)
{
	struct theme *theme = view->server->theme;
	float invisible[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	struct wl_list *part_list = &view->ssd.extents.parts;
	int width = view->w;
	int height = view->h;
	int full_height = height + theme->border_width + SSD_HEIGHT;
	int full_width = width + 2 * theme->border_width;
	int extended_area = EXTENDED_AREA;

	view->ssd.extents.tree = wlr_scene_tree_create(&view->ssd.tree->node);
	struct wlr_scene_node *parent = &view->ssd.extents.tree->node;
	if (view->maximized || view->fullscreen) {
		wlr_scene_node_set_enabled(parent, false);
	}
	wl_list_init(&view->ssd.extents.parts);
	wlr_scene_node_set_position(parent,
		-(theme->border_width + extended_area), -(SSD_HEIGHT + extended_area));

	/* Top */
	add_scene_rect(part_list, LAB_SSD_PART_CORNER_TOP_LEFT, parent,
		extended_area, extended_area,
		0, 0, invisible);
	add_scene_rect(part_list, LAB_SSD_PART_TOP, parent,
		full_width, extended_area,
		extended_area, 0, invisible);
	add_scene_rect(part_list, LAB_SSD_PART_CORNER_TOP_RIGHT, parent,
		extended_area, extended_area,
		extended_area + full_width, 0, invisible);

	/* Sides */
	add_scene_rect(part_list, LAB_SSD_PART_LEFT, parent,
		extended_area, full_height,
		0, extended_area, invisible);
	add_scene_rect(part_list, LAB_SSD_PART_RIGHT, parent,
		extended_area, full_height,
		extended_area + full_width, extended_area, invisible);

	/* Bottom */
	add_scene_rect(part_list, LAB_SSD_PART_CORNER_BOTTOM_LEFT, parent,
		extended_area, extended_area,
		0, extended_area + full_height, invisible);
	add_scene_rect(part_list, LAB_SSD_PART_BOTTOM, parent,
		full_width, extended_area,
		extended_area, extended_area + full_height, invisible);
	add_scene_rect(part_list, LAB_SSD_PART_CORNER_BOTTOM_RIGHT, parent,
		extended_area, extended_area,
		extended_area + full_width, extended_area + full_height, invisible);
}

static void
enable_part(struct view *view, enum ssd_part_type part_type, bool enable)
{
	struct ssd_part *part = ssd_get_part(&view->ssd.extents.parts, part_type);
	assert(part);
	wlr_scene_node_set_enabled(part->node, enable);
}

bool
ssd_extents_maybe_hide_areas(struct view *view)
{
	if (!view->ssd.enabled || !view->output) {
		return true;
	}

	int lx, ly;
	struct wlr_box *usable_area = &view->output->usable_area;
	wlr_scene_node_coords(&view->ssd.extents.tree->node, &lx, &ly);
	double ox = lx;
	double oy = ly;
	wlr_output_layout_output_coords(view->server->output_layout,
		view->output->wlr_output, &ox, &oy);

	struct theme *theme = view->server->theme;
	int width = EXTENDED_AREA * 2 + theme->border_width * 2 + view->w;
	int height = EXTENDED_AREA * 2 + SSD_HEIGHT + view->h + theme->border_width;

	enum ssd_extent_hide_state *state = &view->ssd.state.extents_hidden;

	bool hide_top = oy < usable_area->y;
	bool hide_bottom = oy + height > usable_area->y + usable_area->height;
	bool hide_left = ox < usable_area->x;
	bool hide_right = ox + width > usable_area->x + usable_area->width;

	struct {
		bool hide;
		enum ssd_extent_hide_state area;
		enum ssd_part_type area_parts[3];
	} conf[4] = {
		{
			.hide = hide_top,
			.area = LAB_SSD_EXTENT_HIDE_STATE_TOP,
			.area_parts = {
				LAB_SSD_PART_CORNER_TOP_LEFT,
				LAB_SSD_PART_TOP,
				LAB_SSD_PART_CORNER_TOP_RIGHT,
			}
		}, {
			.hide = hide_bottom,
			.area = LAB_SSD_EXTENT_HIDE_STATE_BOTTOM,
			.area_parts = {
				LAB_SSD_PART_CORNER_BOTTOM_LEFT,
				LAB_SSD_PART_BOTTOM,
				LAB_SSD_PART_CORNER_BOTTOM_RIGHT,
			}
		}, {
			.hide = hide_left,
			.area = LAB_SSD_EXTENT_HIDE_STATE_LEFT,
			.area_parts = {
				LAB_SSD_PART_CORNER_TOP_LEFT,
				LAB_SSD_PART_LEFT,
				LAB_SSD_PART_CORNER_BOTTOM_LEFT,
			}
		}, {
			.hide = hide_right,
			.area = LAB_SSD_EXTENT_HIDE_STATE_RIGHT,
			.area_parts = {
				LAB_SSD_PART_CORNER_TOP_RIGHT,
				LAB_SSD_PART_RIGHT,
				LAB_SSD_PART_CORNER_BOTTOM_RIGHT,
			}
		},
	};
	enum ssd_part_type type;
	for (size_t i = 0; i < 4; i++) {
		if (conf[i].hide) {
			/* Disable extent */
			for (size_t j = 0; j < 3; j++) {
				enable_part(view, conf[i].area_parts[j], false);
			}
			*state |= conf[i].area;
		} else if (*state & conf[i].area) {
			/* Reenable extent */
			for (size_t j = 0; j < 3; j++) {
				type = conf[i].area_parts[j];
				/* Ignore corners that are hidden on the other axis */
				if (type == LAB_SSD_PART_CORNER_TOP_LEFT
						&& (hide_top || hide_left)) {
					continue;
				} else if (type == LAB_SSD_PART_CORNER_TOP_RIGHT
						&& (hide_top || hide_right)) {
					continue;
				} else if (type == LAB_SSD_PART_CORNER_BOTTOM_LEFT
						&& (hide_bottom || hide_left)) {
					continue;
				} else if (type == LAB_SSD_PART_CORNER_BOTTOM_RIGHT
						&& (hide_bottom || hide_right)) {
					continue;
				}
				enable_part(view, type, true);
			}
			*state &= ~conf[i].area;
		}
	}

	return *state == LAB_SSD_EXTENT_HIDE_STATE_ALL;
}

void
ssd_extents_update(struct view *view)
{
	if (view->maximized || view->fullscreen) {
		wlr_scene_node_set_enabled(&view->ssd.extents.tree->node, false);
		return;
	}
	if (!view->ssd.extents.tree->node.state.enabled) {
		wlr_scene_node_set_enabled(&view->ssd.extents.tree->node, true);
	}

	if (ssd_extents_maybe_hide_areas(view)) {
		/* No extents visble */
		return;
	}
	struct theme *theme = view->server->theme;

	int width = view->w;
	int height = view->h;
	int full_height = height + theme->border_width + SSD_HEIGHT;
	int full_width = width + 2 * theme->border_width;
	int extended_area = EXTENDED_AREA;

	struct ssd_part *part;
	struct wlr_scene_rect *rect;
	wl_list_for_each(part, &view->ssd.extents.parts, link) {
		rect = lab_wlr_scene_get_rect(part->node);
		switch (part->type) {
		case LAB_SSD_PART_TOP:
			wlr_scene_rect_set_size(rect, full_width, extended_area);
			continue;
		case LAB_SSD_PART_CORNER_TOP_RIGHT:
			wlr_scene_node_set_position(
				part->node, extended_area + full_width, 0);
			continue;
		case LAB_SSD_PART_LEFT:
			wlr_scene_rect_set_size(rect, extended_area, full_height);
			continue;
		case LAB_SSD_PART_RIGHT:
			wlr_scene_rect_set_size(rect, extended_area, full_height);
			wlr_scene_node_set_position(
				part->node, extended_area + full_width, extended_area);
			continue;
		case LAB_SSD_PART_CORNER_BOTTOM_LEFT:
			wlr_scene_node_set_position(
				part->node, 0, extended_area + full_height);
			continue;
		case LAB_SSD_PART_BOTTOM:
			wlr_scene_rect_set_size(rect, full_width, extended_area);
			wlr_scene_node_set_position(
				part->node, extended_area, extended_area + full_height);
			continue;
		case LAB_SSD_PART_CORNER_BOTTOM_RIGHT:
			wlr_scene_node_set_position(part->node,
				extended_area + full_width, extended_area + full_height);
			continue;
		default:
			continue;
		}
	}
}

void
ssd_extents_destroy(struct view *view)
{
	if (!view->ssd.extents.tree) {
		return;
	}

	ssd_destroy_parts(&view->ssd.extents.parts);
	wlr_scene_node_destroy(&view->ssd.extents.tree->node);
	view->ssd.extents.tree = NULL;
}
