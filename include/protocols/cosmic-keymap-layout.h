/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PROTOCOLS_COSMIC_KEYMAP_LAYOUT_H
#define LABWC_PROTOCOLS_COSMIC_KEYMAP_LAYOUT_H

#include <wayland-server-core.h>

struct lab_cosmic_keymap_layout_manager {
	struct {
		struct wl_signal request_layout;
	} events;

	/* Private */
	struct wl_global *global;
	struct {
		struct wl_listener display_destroy;
	} on;
	struct wl_list keymap_listeners;
	struct wl_list manager_resources;
};

struct lab_cosmic_keymap_layout_manager *lab_cosmic_keymap_layout_manager_create(
	struct wl_display *display, uint32_t version);

#endif /* LABWC_PROTOCOLS_COSMIC_KEYMAP_LAYOUT_H */

