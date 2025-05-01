// SPDX-License-Identifier: GPL-2.0-only

#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_scene.h>
#include "config.h"
#include "ssd-internal.h"
#include "theme.h"

/* no gradient, rendering two rounded corners + wlr_scene_rect */
static void
create(struct ssd *ssd, struct ssd_sub_tree *subtree, int width)
{
	// FIXME: rounded corner buffers missing
	struct theme *theme = rc.theme;
	int active = (subtree == &ssd->titlebar.active) ? THEME_ACTIVE : THEME_INACTIVE;

	struct ssd_part *part =
		add_scene_part(&subtree->parts, LAB_SSD_PART_TITLEBAR);
	struct wlr_scene_rect *titlebar = wlr_scene_rect_create(
		subtree->tree, width + theme->border_width * 2,
		theme->titlebar_height + theme->border_width,
		theme->window[active].title_bg.color);
	wlr_scene_node_set_position(&titlebar->node,
		-theme->border_width, -theme->border_width);
	part->node = &titlebar->node;
}

static void
set_size(struct ssd *ssd, struct ssd_sub_tree *subtree, int width, int height)
{
	struct theme *theme = rc.theme;
	struct ssd_part *part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLEBAR);
	struct wlr_scene_rect *titlebar_rect = wlr_scene_rect_from_node(part->node);
	wlr_scene_rect_set_size(titlebar_rect,
		width + theme->border_width * 2,
		height + theme->border_width);
}

static void
set_square(struct ssd *ssd, struct ssd_sub_tree *subtree, bool enabled)
{
	// FIXME: implement
	// update position and width + toggle visibility of corner buttons
}

struct ssd_title_bg_impl
ssd_titlebar_bg_rect_init(void)
{
	return (struct ssd_title_bg_impl) {
		.create = create,
		.set_size = set_size,
		.square_corners = set_square,
	};
}
