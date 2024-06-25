// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include "common/list.h"
#include "common/mem.h"
#include "labwc.h"
#include "node.h"
#include "view.h"
#include "view-impl-common.h"
#include "workspaces.h"

#define LAB_TAB_HEIGHT 40

static struct multi_view *
multi_view_from_view(struct view *view)
{
	assert(view->type == LAB_MULTI_VIEW);
	return (struct multi_view *)view;
}

static void
multi_view_configure(struct view *view, struct wlr_box geo)
{
	struct multi_view *multi_view = multi_view_from_view(view);
	view->current = geo;
	view->pending = geo;
	wlr_scene_rect_set_size(
		multi_view->content.bg_tabs, geo.width, LAB_TAB_HEIGHT);
	wlr_scene_rect_set_size(
		multi_view->content.bg_surfaces, geo.width, geo.height - LAB_TAB_HEIGHT);
	view_moved(view);
}

static bool
multi_view_is_focusable(struct view *view)
{
	return true;
}

static const struct view_impl multi_view_impl = {
	.configure = multi_view_configure,
	.move_to_front = view_impl_move_to_front,
	.move_to_back = view_impl_move_to_back,
	.is_focusable = multi_view_is_focusable,
};

static void
create_layout(struct multi_view *multi_view)
{
	float bg_tabs[4] = { 0.3, 0.3, 0.3, 1};
	float bg_surfaces[4] = { 0.6, 0.6, 0.6, 1};

	multi_view->content.tree = wlr_scene_tree_create(multi_view->base.scene_tree);
	multi_view->base.scene_node = &multi_view->content.tree->node;

	multi_view->content.tabs = wlr_scene_tree_create(multi_view->content.tree);
	multi_view->content.bg_tabs = wlr_scene_rect_create(
		multi_view->content.tabs, 0, 0, bg_tabs);

	multi_view->content.surfaces = wlr_scene_tree_create(multi_view->content.tree);
	multi_view->content.bg_surfaces = wlr_scene_rect_create(
		multi_view->content.surfaces, 0, 0, bg_surfaces);

	wlr_scene_node_set_position(&multi_view->content.surfaces->node, 0, LAB_TAB_HEIGHT);
}

struct view *
multi_view_create(struct server *server)
{
	struct multi_view *multi_view = znew(*multi_view);
	struct view *view = &multi_view->base;

	/* Init multi view */
	view->server = server;
	view->type = LAB_MULTI_VIEW;
	view->impl = &multi_view_impl;
	wl_list_init(&multi_view->views);

	view->workspace = server->workspace_current;
	view->scene_tree = wlr_scene_tree_create(view->workspace->tree);
	node_descriptor_create(&view->scene_tree->node, LAB_NODE_DESC_VIEW, view);

	create_layout(multi_view);

	multi_view_configure(view, (struct wlr_box){ 0, 0, 640, 480 });
	view_place_by_policy(view, /* allow_cursor */ true, rc.placement_policy);

	view_set_ssd_mode(view, LAB_SSD_MODE_FULL);

	wl_list_append(&server->views, &view->link);

	wlr_log(WLR_ERROR, "Creating view %p", view);
	view->mapped = true;
	return view;
}
