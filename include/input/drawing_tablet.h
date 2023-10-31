/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_DRAWING_TABLET_H
#define LABWC_DRAWING_TABLET_H

#include <wayland-server-core.h>
struct wlr_device;

struct drawing_tablet {
	struct wlr_tablet *tablet;
	struct {
		struct wl_listener axis;
		struct wl_listener proximity;
		struct wl_listener tip;
		struct wl_listener button;
		struct wl_listener destroy;
	} handlers;
};


void drawing_tablet_setup_handlers(struct wlr_input_device *wlr_input_device);

#endif /* LABWC_DRAWING_TABLET_H */
