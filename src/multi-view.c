// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include "common/list.h"
#include "common/mem.h"
#include "labwc.h"
#include "node.h"
#include "view.h"
#include "view-impl-common.h"
#include "workspaces.h"
#include "ui/tabs.h"

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

	tab_view_configure(multi_view->tab_view, geo);

	geo.height -= LAB_TAB_HEIGHT;
	wl_list_for_each(view, &multi_view->views, link) {
		if (view->impl->configure) {
			view->impl->configure(view, geo);
		}
	}

	view_moved(&multi_view->base);
}

static bool
multi_view_is_focusable(struct view *view)
{
	return true;
}

static const char *
multi_view_get_string_prop(struct view *view, const char *prop)
{
	struct multi_view *multi_view = multi_view_from_view(view);

	view = multi_view->selected;
	if (view && view->impl->get_string_prop) {
		return view->impl->get_string_prop(view, prop);
	}

	return "";
}

static const struct view_impl multi_view_impl = {
	.configure = multi_view_configure,
	.move_to_front = view_impl_move_to_front,
	.move_to_back = view_impl_move_to_back,
	.is_focusable = multi_view_is_focusable,
	.get_string_prop = multi_view_get_string_prop,
};

static void
handle_tab_view_selection(struct wl_listener *listener, void *previous_selection)
{
	struct multi_view *multi_view = wl_container_of(listener, multi_view, on.selection);
	struct wlr_scene_node *node = multi_view->tab_view->selected;
	//struct wlr_scene_node *old = previous_selection;
	wlr_log(WLR_INFO, "selection changed to %p", node);
	if (!node) {
		multi_view->selected = NULL;
		multi_view->base.surface = NULL;
		ssd_update_geometry(multi_view->base.ssd);
		return;
	}
	struct view *view = NULL;
	wl_list_for_each(view, &multi_view->views, link) {
		if (view->scene_node == node) {
			multi_view->selected = view;
			multi_view->base.surface = view->surface;
			ssd_update_geometry(multi_view->base.ssd);
			multi_view_configure(&multi_view->base, multi_view->base.pending);
			return;
		}
	}
	wlr_log(WLR_ERROR, "no view found for scene node %p", node);
	assert(false && "No view found for scene node");
}

static void
create_layout(struct multi_view *multi_view)
{
	multi_view->tab_view = tab_view_create(multi_view->base.scene_tree, LAB_TAB_HEIGHT);
	multi_view->on.selection.notify = handle_tab_view_selection;
	wl_signal_add(&multi_view->tab_view->events.selection, &multi_view->on.selection);
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

static void
multi_view_remove(struct multi_view *multi_view, struct view *view)
{
	assert(multi_view);
	assert(view);

	tab_view_remove(multi_view->tab_view, view->scene_node, view->scene_tree);

	view_set_ssd_mode(view, LAB_SSD_MODE_FULL);
	wl_list_remove(&view->link);
	wl_list_insert(&view->server->views, &view->link);
	desktop_focus_view(view, /* raise */ true);
}

void
multi_view_add(struct multi_view *multi_view, struct view *view)
{
	assert(multi_view);
	assert(view);

	if (view->type == LAB_MULTI_VIEW) {
		wlr_log(WLR_ERROR, "Removing tabbed view");
		if (multi_view->selected) {
			multi_view_remove(multi_view, multi_view->selected);
		}
		return;
	}
	if (tab_view_contains(multi_view->tab_view, view->scene_node)) {
		wlr_log(WLR_ERROR, "already a tabbed view. removing");
		multi_view_remove(multi_view, view);
		return;
	}

	view_set_ssd_mode(view, LAB_SSD_MODE_NONE);

	/* FIXME: This doesn't work view_impl_move_to_front() will reattach to server->views */
	/*        We could use ->content->surfaces and then go via view = node->data instead */
	wl_list_remove(&view->link);
	wl_list_append(&multi_view->views, &view->link);

	tab_view_add(multi_view->tab_view, view->scene_node);
	desktop_focus_view(&multi_view->base, /* raise */ true);
}

bool
multi_view_cycle(struct multi_view *multi_view)
{
	return tab_view_cycle(multi_view->tab_view);
}
