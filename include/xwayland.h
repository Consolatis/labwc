/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_XWAYLAND_H
#define LABWC_XWAYLAND_H
#include "config.h"
#if HAVE_XWAYLAND
#include "view.h"
#include <xcb/xcb.h>

/**
* This is in xcb/xcb_event.h, but pulling xcb-util
* just for a constant others redefine anyway is meh
*/
#define XCB_EVENT_RESPONSE_TYPE_MASK 0x7f

struct wlr_compositor;

struct xwayland_unmanaged {
	struct server *server;
	struct wlr_xwayland_surface *xwayland_surface;
	struct wlr_scene_node *node;
	struct wl_list link;

	struct wl_listener request_activate;
	struct wl_listener request_configure;
/*	struct wl_listener request_fullscreen; */
	struct wl_listener set_geometry;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener override_redirect;
};

struct xwayland_view {
	struct view base;
	struct wlr_xwayland_surface *xwayland_surface;

	/* Events unique to XWayland views */
	struct wl_listener request_activate;
	struct wl_listener request_configure;
	struct wl_listener set_app_id;		/* TODO: s/set_app_id/class/ */
	struct wl_listener set_decorations;
	struct wl_listener override_redirect;

	/* Not (yet) implemented */
/*	struct wl_listener set_role; */
/*	struct wl_listener set_window_type; */
/*	struct wl_listener set_hints; */
};

void xwayland_unmanaged_create(struct server *server,
	struct wlr_xwayland_surface *xsurface, bool mapped);

void xwayland_view_create(struct server *server,
	struct wlr_xwayland_surface *xsurface, bool mapped);

struct wlr_xwayland_surface *xwayland_surface_from_view(struct view *view);

void xwayland_server_init(struct server *server,
	struct wlr_compositor *compositor);
void xwayland_server_finish(struct server *server);


/* https://specifications.freedesktop.org/wm-spec/wm-spec-1.4.html#idm45649101374512 */
enum atom {
	NET_WM_WINDOW_TYPE = 0,
	NET_WM_WINDOW_TYPE_DESKTOP,
	NET_WM_WINDOW_TYPE_DOCK,
	NET_WM_WINDOW_TYPE_TOOLBAR,
	NET_WM_WINDOW_TYPE_MENU,
	NET_WM_WINDOW_TYPE_UTILITY,
	NET_WM_WINDOW_TYPE_SPLASH,
	NET_WM_WINDOW_TYPE_DIALOG,
	NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
	NET_WM_WINDOW_TYPE_POPUP_MENU,
	NET_WM_WINDOW_TYPE_TOOLTIP,
	NET_WM_WINDOW_TYPE_NOTIFICATION,
	NET_WM_WINDOW_TYPE_COMBO,
	NET_WM_WINDOW_TYPE_DND,
	NET_WM_WINDOW_TYPE_NORMAL,
	ATOM_NON_EXISTING_TESTCASE_FOO,
	ATOM_LEN
};

static const char *const atom_names[ATOM_LEN] = {
	"_NET_WM_WINDOW_TYPE",
	"_NET_WM_WINDOW_TYPE_DESKTOP",
	"_NET_WM_WINDOW_TYPE_DOCK",
	"_NET_WM_WINDOW_TYPE_TOOLBAR",
	"_NET_WM_WINDOW_TYPE_MENU",
	"_NET_WM_WINDOW_TYPE_UTILITY",
	"_NET_WM_WINDOW_TYPE_SPLASH",
	"_NET_WM_WINDOW_TYPE_DIALOG",
	"_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
	"_NET_WM_WINDOW_TYPE_POPUP_MENU",
	"_NET_WM_WINDOW_TYPE_TOOLTIP",
	"_NET_WM_WINDOW_TYPE_NOTIFICATION",
	"_NET_WM_WINDOW_TYPE_COMBO",
	"_NET_WM_WINDOW_TYPE_DND",
	"_NET_WM_WINDOW_TYPE_NORMAL",
	"FooBARbazBar",
};

extern xcb_atom_t atoms[ATOM_LEN];

#endif /* HAVE_XWAYLAND */
#endif /* LABWC_XWAYLAND_H */
