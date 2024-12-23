/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PROTOCOLS_EXT_WORKSPACES_FOO_H
#define LABWC_PROTOCOLS_EXT_WORKSPACES_FOO_H

#include <wayland-server-core.h>

struct lab_ext_workspace;
struct wlr_foreign_toplevel_handle_v1;

struct interop_manager {
	struct wl_global *global;
	struct wl_list handles;
	struct wl_list resources;
	struct {
		struct wl_listener display_destroy;
	} on;
};

struct interop_handle {
	struct interop_manager *manager;
	struct wlr_ext_foreign_toplevel_handle_v1 *toplevel;
	//struct wlr_foreign_toplevel_handle_v1 *toplevel;
	struct wl_list resources;
	struct wl_list mappings;
	struct {
		struct wl_listener toplevel_destroy;
	} on;
	struct {
		struct wl_signal request_join_workspace;
		struct wl_signal request_leave_workspace;
	} events;
	struct wl_list link;
};


struct interop_manager *interop_manager_create(
	struct wl_display *display, uint32_t version);

struct interop_handle *interop_handle_create(
	struct interop_manager *manager,
	struct wlr_ext_foreign_toplevel_handle_v1 *toplevel);
	//struct wlr_foreign_toplevel_handle_v1 *toplevel);

void toplevel_join_workspace(
	struct interop_handle *handle, struct lab_ext_workspace *workspace);

void toplevel_leave_workspace(
	struct interop_handle *handle, struct lab_ext_workspace *workspace);

#endif /* LABWC_PROTOCOLS_EXT_WORKSPACES_H */
