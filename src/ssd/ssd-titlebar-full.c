// SPDX-License-Identifier: GPL-2.0-only

#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_scene.h>
#include "config.h"
#include "common/scaled-titlebar-buffer.h"
#include "ssd-internal.h"
#include "theme.h"


/* horizontal gradient, rendering the whole titlebar at once */
static void
create(struct ssd *ssd, struct ssd_sub_tree *subtree, int width)
{
	struct theme *theme = rc.theme;
	int active = (subtree == &ssd->titlebar.active) ? THEME_ACTIVE : THEME_INACTIVE;

	struct ssd_part *part =
		add_scene_part(&subtree->parts, LAB_SSD_PART_TITLEBAR);
	struct scaled_titlebar_buffer *titlebar = scaled_titlebar_buffer_create(
		subtree->tree, width + theme->border_width * 2,
		theme->titlebar_height + theme->border_width,
		theme->border_width, rc.corner_radius,
		theme->window[active].titlebar_pattern,
		theme->window[active].border_color);
	wlr_scene_node_set_position(&titlebar->scene_buffer->node,
		-theme->border_width, -theme->border_width);
	part->node = &titlebar->scene_buffer->node;
}

static void
set_size(struct ssd *ssd, struct ssd_sub_tree *subtree, int width, int height)
{
	struct theme *theme = rc.theme;
	struct ssd_part *part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLEBAR);
	struct scaled_titlebar_buffer *titlebar = scaled_titlebar_buffer_from_node(part->node);
	scaled_titlebar_buffer_set_size(titlebar,
		width + theme->border_width * 2,
		height + theme->border_width);
}

static void
set_square(struct ssd *ssd, struct ssd_sub_tree *subtree, bool enable)
{
	struct ssd_part *part = ssd_get_part(&subtree->parts, LAB_SSD_PART_TITLEBAR);
	struct scaled_titlebar_buffer *titlebar = scaled_titlebar_buffer_from_node(part->node);
	scaled_titlebar_buffer_set_square(titlebar, enable);
}

struct ssd_title_bg_impl
ssd_titlebar_bg_full_init(void)
{
	return (struct ssd_title_bg_impl) {
		.create = create,
		.set_size = set_size,
		.square_corners = set_square,
	};
}
