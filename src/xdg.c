// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include "common/mem.h"
#include "labwc.h"
#include "node.h"
#include "ssd.h"
#include "view.h"
#include "workspaces.h"

struct wlr_xdg_surface *
xdg_surface_from_view(struct view *view)
{
	assert(view->type == LAB_XDG_SHELL_VIEW);
	struct xdg_toplevel_view *xdg_toplevel_view =
		(struct xdg_toplevel_view *)view;
	assert(xdg_toplevel_view->xdg_surface);
	return xdg_toplevel_view->xdg_surface;
}

static struct wlr_xdg_toplevel *
xdg_toplevel_from_view(struct view *view)
{
	struct wlr_xdg_surface *xdg_surface = xdg_surface_from_view(view);
	assert(xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);
	assert(xdg_surface->toplevel);
	return xdg_surface->toplevel;
}

static void
handle_new_xdg_popup(struct wl_listener *listener, void *data)
{
	struct xdg_toplevel_view *xdg_toplevel_view =
		wl_container_of(listener, xdg_toplevel_view, new_popup);
	struct view *view = &xdg_toplevel_view->base;
	struct wlr_xdg_popup *wlr_popup = data;
	xdg_popup_create(view, wlr_popup);
}

static bool
has_ssd(struct view *view)
{
	if (!rc.xdg_shell_server_side_deco) {
		return false;
	}

	/*
	 * Some XDG shells refuse to disable CSD in which case their
	 * geometry.{x,y} seems to be greater than zero. We filter on that
	 * on the assumption that this will remain true.
	 */
	struct wlr_xdg_surface_state *current =
		&xdg_surface_from_view(view)->current;
	if (current->geometry.x || current->geometry.y) {
		return false;
	}
	return true;
}

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, commit);
	struct wlr_xdg_surface *xdg_surface = xdg_surface_from_view(view);
	assert(view->surface);

	struct wlr_box size;
	wlr_xdg_surface_get_geometry(xdg_surface, &size);

	struct wlr_box *current = &view->current;
	struct wlr_box *pending = &view->pending;
	bool update_required = false;

	if (current->width != size.width || current->height != size.height) {
		update_required = true;
		current->width = size.width;
		current->height = size.height;
	}

	uint32_t serial = view->pending_configure_serial;
	if (serial > 0 && serial >= xdg_surface->current.configure_serial) {
		if (current->x != pending->x) {
			update_required = true;
			current->x = pending->x + pending->width - size.width;
		}
		if (current->y != pending->y) {
			update_required = true;
			current->y = pending->y + pending->height - size.height;
		}
		if (serial == xdg_surface->current.configure_serial) {
			view->pending_configure_serial = 0;
		}
	}
	if (update_required) {
		view_moved(view);
	}
}

static void
handle_map(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, map);
	view->impl->map(view);
}

static void
handle_unmap(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, unmap);
	view->impl->unmap(view);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, destroy);
	assert(view->type == LAB_XDG_SHELL_VIEW);

	/* Reset XDG specific surface for good measure */
	((struct xdg_toplevel_view *)view)->xdg_surface = NULL;

	/* Remove XDG specific handlers */
	wl_list_remove(&view->destroy.link);

	/* And finally destroy / free the view */
	view_destroy(view);
}

static void
handle_request_move(struct wl_listener *listener, void *data)
{
	/*
	 * This event is raised when a client would like to begin an interactive
	 * move, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provied serial against a list of button press serials sent to
	 * this
	 * client, to prevent the client from requesting this whenever they
	 * want.
	 */
	struct view *view = wl_container_of(listener, view, request_move);
	interactive_begin(view, LAB_INPUT_STATE_MOVE, 0);
}

static void
handle_request_resize(struct wl_listener *listener, void *data)
{
	/*
	 * This event is raised when a client would like to begin an interactive
	 * resize, typically because the user clicked on their client-side
	 * decorations. Note that a more sophisticated compositor should check
	 * the provied serial against a list of button press serials sent to
	 * this
	 * client, to prevent the client from requesting this whenever they
	 * want.
	 */
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct view *view = wl_container_of(listener, view, request_resize);
	interactive_begin(view, LAB_INPUT_STATE_RESIZE, event->edges);
}

static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_minimize);
	view_minimize(view, xdg_toplevel_from_view(view)->requested.minimized);
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_maximize);
	if (!view->mapped) {
		return;
	}
	view_maximize(view, xdg_toplevel_from_view(view)->requested.maximized,
		/*store_natural_geometry*/ true);
}

static void
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, request_fullscreen);
	if (!view->mapped) {
		return;
	}
	struct wlr_xdg_toplevel *xdg_toplevel = xdg_toplevel_from_view(view);
	view_set_fullscreen(view, xdg_toplevel->requested.fullscreen,
		xdg_toplevel->requested.fullscreen_output);
}

static void
handle_set_title(struct wl_listener *listener, void *data)
{
	struct view *view = wl_container_of(listener, view, set_title);
	assert(view);
	view_update_title(view);
}

static void
handle_set_app_id(struct wl_listener *listener, void *data)
{
	struct xdg_toplevel_view *xdg_toplevel_view =
		wl_container_of(listener, xdg_toplevel_view, set_app_id);
	struct view *view = &xdg_toplevel_view->base;
	assert(view);
	view_update_app_id(view);
}

static void
xdg_toplevel_view_configure(struct view *view, struct wlr_box geo)
{
	view_adjust_size(view, &geo.width, &geo.height);
	view->pending = geo;

	struct wlr_xdg_toplevel *xdg_toplevel = xdg_toplevel_from_view(view);
	uint32_t serial = wlr_xdg_toplevel_set_size(xdg_toplevel,
		(uint32_t)geo.width, (uint32_t)geo.height);
	if (serial > 0) {
		view->pending_configure_serial = serial;
	} else if (view->pending_configure_serial == 0) {
		view->current.x = geo.x;
		view->current.y = geo.y;
		view_moved(view);
	}
}

static void
xdg_toplevel_view_move(struct view *view, int x, int y)
{
	view->current.x = x;
	view->current.y = y;

	/* override any previous pending move */
	view->pending.x = x;
	view->pending.y = y;

	view_moved(view);
}

static void
xdg_toplevel_view_close(struct view *view)
{
	wlr_xdg_toplevel_send_close(xdg_toplevel_from_view(view));
}

static void
xdg_toplevel_view_maximize(struct view *view, bool maximized)
{
	wlr_xdg_toplevel_set_maximized(xdg_toplevel_from_view(view), maximized);
}

static void
xdg_toplevel_view_set_activated(struct view *view, bool activated)
{
	wlr_xdg_toplevel_set_activated(xdg_toplevel_from_view(view), activated);
}

static void
xdg_toplevel_view_set_fullscreen(struct view *view, bool fullscreen)
{
	wlr_xdg_toplevel_set_fullscreen(xdg_toplevel_from_view(view),
		fullscreen);
}

static struct view *
lookup_view_by_xdg_toplevel(struct server *server,
		struct wlr_xdg_toplevel *xdg_toplevel)
{
	struct view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->type != LAB_XDG_SHELL_VIEW) {
			continue;
		}
		if (xdg_toplevel_from_view(view) == xdg_toplevel) {
			return view;
		}
	}
	return NULL;
}

static void
position_xdg_toplevel_view(struct view *view)
{
	struct wlr_xdg_surface *xdg_surface = xdg_surface_from_view(view);
	struct wlr_xdg_toplevel *parent_xdg_toplevel =
		xdg_toplevel_from_view(view)->parent;

	if (!parent_xdg_toplevel) {
		struct wlr_box box =
			output_usable_area_from_cursor_coords(view->server);
		view->current.x = box.x;
		view->current.y = box.y;
		view_center(view);
	} else {
		/*
		 * If child-toplevel-views, we center-align relative to their
		 * parents
		 */
		struct view *parent = lookup_view_by_xdg_toplevel(
			view->server, parent_xdg_toplevel);
		assert(parent);
		int center_x = parent->current.x + parent->current.width / 2;
		int center_y = parent->current.y + parent->current.height / 2;
		view->current.x = center_x
			- xdg_surface->current.geometry.width / 2;
		view->current.y = center_y
			- xdg_surface->current.geometry.height / 2;
	}

	struct border margin = ssd_get_margin(view->ssd);
	view->current.x += margin.left;
	view->current.y += margin.top;
}

static const char *
xdg_toplevel_view_get_string_prop(struct view *view, const char *prop)
{
	struct wlr_xdg_toplevel *xdg_toplevel = xdg_toplevel_from_view(view);
	if (!strcmp(prop, "title")) {
		return xdg_toplevel->title;
	}
	if (!strcmp(prop, "app_id")) {
		return xdg_toplevel->app_id;
	}
	return "";
}

static void
xdg_toplevel_view_map(struct view *view)
{
	if (view->mapped) {
		return;
	}
	view->mapped = true;
	struct wlr_xdg_surface *xdg_surface = xdg_surface_from_view(view);
	view->surface = xdg_surface->surface;
	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	if (!view->been_mapped) {
		struct wlr_xdg_toplevel_requested *requested =
			&xdg_toplevel_from_view(view)->requested;
		foreign_toplevel_handle_create(view);
		view_set_decorations(view, has_ssd(view));

		/*
		 * Set initial "pending" dimensions (may be modified by
		 * view_set_fullscreen/view_maximize() below). "Current"
		 * dimensions remain zero until handle_commit().
		 */
		view->pending.width = xdg_surface->current.geometry.width;
		view->pending.height = xdg_surface->current.geometry.height;

		position_xdg_toplevel_view(view);
		if (!view->fullscreen && requested->fullscreen) {
			view_set_fullscreen(view, true,
				requested->fullscreen_output);
		} else if (!view->maximized && requested->maximized) {
			view_maximize(view, true,
				/*store_natural_geometry*/ true);
		}

		view_moved(view);
		view->been_mapped = true;
	}

	view->commit.notify = handle_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &view->commit);

	view_impl_map(view);
}

static void
xdg_toplevel_view_unmap(struct view *view)
{
	if (view->mapped) {
		view->mapped = false;
		wlr_scene_node_set_enabled(&view->scene_tree->node, false);
		wl_list_remove(&view->commit.link);
		desktop_focus_topmost_mapped_view(view->server);
	}
}

static const struct view_impl xdg_toplevel_view_impl = {
	.configure = xdg_toplevel_view_configure,
	.close = xdg_toplevel_view_close,
	.get_string_prop = xdg_toplevel_view_get_string_prop,
	.map = xdg_toplevel_view_map,
	.move = xdg_toplevel_view_move,
	.set_activated = xdg_toplevel_view_set_activated,
	.set_fullscreen = xdg_toplevel_view_set_fullscreen,
	.unmap = xdg_toplevel_view_unmap,
	.maximize = xdg_toplevel_view_maximize,
};

/*
 * We use the following struct user_data pointers:
 *   - wlr_xdg_surface->data = view
 *     for the wlr_xdg_toplevel_decoration_v1 implementation
 *   - wlr_surface->data = scene_tree
 *     to help the popups find their parent nodes
 */
void
xdg_surface_new(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, new_xdg_surface);
	struct wlr_xdg_surface *xdg_surface = data;

	/*
	 * We deal with popups in xdg-popup.c and layers.c as they have to be
	 * treated differently
	 */
	if (xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return;
	}

	wlr_xdg_surface_ping(xdg_surface);

	struct xdg_toplevel_view *xdg_toplevel_view = znew(*xdg_toplevel_view);
	struct view *view = &xdg_toplevel_view->base;

	view->server = server;
	view->type = LAB_XDG_SHELL_VIEW;
	view->impl = &xdg_toplevel_view_impl;
	xdg_toplevel_view->xdg_surface = xdg_surface;

	view->workspace = server->workspace_current;
	view->scene_tree = wlr_scene_tree_create(view->workspace->tree);
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);

	struct wlr_scene_tree *tree = wlr_scene_xdg_surface_create(
		view->scene_tree, xdg_surface);
	if (!tree) {
		/* TODO: might need further clean up */
		wl_resource_post_no_memory(xdg_surface->resource);
		free(xdg_toplevel_view);
		return;
	}
	view->scene_node = &tree->node;
	node_descriptor_create(&view->scene_tree->node,
		LAB_NODE_DESC_VIEW, view);

	/* In support of xdg_toplevel_decoration */
	xdg_surface->data = view;

	/* In support of xdg popups */
	xdg_surface->surface->data = tree;

	view->map.notify = handle_map;
	wl_signal_add(&xdg_surface->events.map, &view->map);
	view->unmap.notify = handle_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &view->unmap);
	view->destroy.notify = handle_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &view->destroy);

	struct wlr_xdg_toplevel *toplevel = xdg_surface->toplevel;
	view->request_move.notify = handle_request_move;
	wl_signal_add(&toplevel->events.request_move, &view->request_move);
	view->request_resize.notify = handle_request_resize;
	wl_signal_add(&toplevel->events.request_resize, &view->request_resize);
	view->request_minimize.notify = handle_request_minimize;
	wl_signal_add(&toplevel->events.request_minimize, &view->request_minimize);
	view->request_maximize.notify = handle_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize, &view->request_maximize);
	view->request_fullscreen.notify = handle_request_fullscreen;
	wl_signal_add(&toplevel->events.request_fullscreen, &view->request_fullscreen);

	view->set_title.notify = handle_set_title;
	wl_signal_add(&toplevel->events.set_title, &view->set_title);

	/* Events specific to XDG toplevel views */
	xdg_toplevel_view->set_app_id.notify = handle_set_app_id;
	wl_signal_add(&toplevel->events.set_app_id, &xdg_toplevel_view->set_app_id);

	xdg_toplevel_view->new_popup.notify = handle_new_xdg_popup;
	wl_signal_add(&xdg_surface->events.new_popup, &xdg_toplevel_view->new_popup);

	wl_list_insert(&server->views, &view->link);
}
