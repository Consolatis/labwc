// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include "protocols/workspace_interop.h"
#include "common/macros.h"
#include "labwc.h"
#include "view.h"
#include "workspaces.h"
#include "foreign-toplevel-internal.h"

/* ext signals */
static void
handle_handle_destroy(struct wl_listener *listener, void *data)
{
	struct ext_foreign_toplevel *ext_toplevel =
		wl_container_of(listener, ext_toplevel, on.handle_destroy);

	/* Client side requests */
	wl_list_remove(&ext_toplevel->on.handle_destroy.link);
	ext_toplevel->handle = NULL;
}

/* Compositor signals */
static void
handle_new_app_id(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel =
		wl_container_of(listener, toplevel, ext_toplevel.on_view.new_app_id);
	assert(toplevel->ext_toplevel.handle);

	struct wlr_ext_foreign_toplevel_handle_v1_state state = {
		.title = view_get_string_prop(toplevel->view, "title"),
		.app_id = view_get_string_prop(toplevel->view, "app_id")
	};
	wlr_ext_foreign_toplevel_handle_v1_update_state(
		toplevel->ext_toplevel.handle, &state);
}

static void
handle_new_title(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel =
		wl_container_of(listener, toplevel, ext_toplevel.on_view.new_title);
	assert(toplevel->ext_toplevel.handle);

	struct wlr_ext_foreign_toplevel_handle_v1_state state = {
		.title = view_get_string_prop(toplevel->view, "title"),
		.app_id = view_get_string_prop(toplevel->view, "app_id")
	};
	wlr_ext_foreign_toplevel_handle_v1_update_state(
		toplevel->ext_toplevel.handle, &state);
}

static void
handle_workspace_changed(struct wl_listener *listener, void *data)
{
	struct foreign_toplevel *toplevel =
		wl_container_of(listener, toplevel, ext_toplevel.on_view.workspace_changed);
	assert(toplevel->ext_toplevel.interop_handle);

	struct workspace *new_workspace = data;
	toplevel_leave_workspace(
		toplevel->ext_toplevel.interop_handle,
		toplevel->view->workspace->ext_workspace);
	toplevel_join_workspace(
		toplevel->ext_toplevel.interop_handle, new_workspace->ext_workspace);
}

/* Internal signals */
static void
handle_toplevel_destroy(struct wl_listener *listener, void *data)
{
	struct ext_foreign_toplevel *ext_toplevel =
		wl_container_of(listener, ext_toplevel, on_foreign_toplevel.toplevel_destroy);

	if (!ext_toplevel->handle) {
		return;
	}

	wlr_ext_foreign_toplevel_handle_v1_destroy(ext_toplevel->handle);

	/* interop_handle has its own toplevel destroy listener */
	ext_toplevel->interop_handle = NULL;

	/* Compositor side state changes */
	wl_list_remove(&ext_toplevel->on_view.new_app_id.link);
	wl_list_remove(&ext_toplevel->on_view.new_title.link);
	wl_list_remove(&ext_toplevel->on_view.workspace_changed.link);

	/* Internal signals */
	wl_list_remove(&ext_toplevel->on_foreign_toplevel.toplevel_destroy.link);
}

/* Internal API */
void
ext_foreign_toplevel_init(struct foreign_toplevel *toplevel)
{
	struct ext_foreign_toplevel *ext_toplevel = &toplevel->ext_toplevel;
	struct view *view = toplevel->view;

	assert(view->server->foreign_toplevel_list);

	struct wlr_ext_foreign_toplevel_handle_v1_state state = {
		.title = view_get_string_prop(toplevel->view, "title"),
		.app_id = view_get_string_prop(toplevel->view, "app_id")
	};
	ext_toplevel->handle = wlr_ext_foreign_toplevel_handle_v1_create(
		view->server->foreign_toplevel_list, &state);

	if (!ext_toplevel->handle) {
		wlr_log(WLR_ERROR, "cannot create ext toplevel handle for (%s)",
			view_get_string_prop(view, "title"));
		return;
	}

	ext_toplevel->interop_handle = interop_handle_create(
		view->server->interop_manager, ext_toplevel->handle);
	if (!ext_toplevel->interop_handle) {
		wlr_log(WLR_ERROR, "cannot create interop handle for (%s)",
			view_get_string_prop(view, "title"));
		return;
	}
	toplevel_join_workspace(ext_toplevel->interop_handle,
		toplevel->view->workspace->ext_workspace);

	/* Client side requests */
	ext_toplevel->on.handle_destroy.notify = handle_handle_destroy;
	wl_signal_add(&ext_toplevel->handle->events.destroy, &ext_toplevel->on.handle_destroy);

	/* Compositor side state changes */
	CONNECT_SIGNAL(view, &ext_toplevel->on_view, new_app_id);
	CONNECT_SIGNAL(view, &ext_toplevel->on_view, new_title);
	CONNECT_SIGNAL(view, &ext_toplevel->on_view, workspace_changed);

	/* Internal signals */
	CONNECT_SIGNAL(toplevel, &ext_toplevel->on_foreign_toplevel, toplevel_destroy);
}
