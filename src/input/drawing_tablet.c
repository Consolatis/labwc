// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/util/log.h>
#include "common/macros.h"
#include "common/mem.h"
#include "input/drawing_tablet.h"

static void
setup_pad(struct wlr_input_device *wlr_device)
{
	wlr_log(WLR_ERROR, "not setting up pad");
}

static void
handle_axis(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_axis_event *ev = data;
	wlr_log(WLR_DEBUG, "Got axis event: %.10f,%1.10f", ev->x, ev->y);
}

static void
handle_proximity(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_proximity_event *ev = data;
	wlr_log(WLR_DEBUG, "Got proximity event:");
	wlr_log(WLR_DEBUG, "\t %.10f,%1.10f", ev->x, ev->y);
	wlr_log(WLR_DEBUG, "\t state %s",
		ev->state == WLR_TABLET_TOOL_PROXIMITY_IN ? "in" : "out");
}

static void
handle_tip(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_tip_event *ev = data;
	wlr_log(WLR_DEBUG, "Got tip event:");
	wlr_log(WLR_DEBUG, "\t %.10f,%1.10f", ev->x, ev->y);
	wlr_log(WLR_DEBUG, "\t state %s",
		ev->state == WLR_TABLET_TOOL_TIP_UP ? "up" : "down");
}

static void
handle_button(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_button_event *ev = data;
	wlr_log(WLR_DEBUG, "Got button event: %u %s", ev->button,
		ev->state == WLR_BUTTON_RELEASED ? "release" : "press");
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct drawing_tablet *tablet =
		wl_container_of(listener, tablet, handlers.destroy);
	wlr_log(WLR_ERROR, "Destroying tablet");
	free(tablet);
}

static void
setup_pen(struct wlr_input_device *wlr_device)
{
	wlr_log(WLR_INFO, "setting up tablet");
	struct drawing_tablet *tablet = znew(*tablet);
	tablet->tablet = wlr_tablet_from_input_device(wlr_device);
	tablet->tablet->data = tablet;
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, axis);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, proximity);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, tip);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, button);
	CONNECT_SIGNAL(wlr_device, &tablet->handlers, destroy);
}

void
drawing_tablet_setup_handlers(struct wlr_input_device *device)
{
	switch(device->type) {
	case WLR_INPUT_DEVICE_TABLET_PAD:
		setup_pad(device);
		break;
	case WLR_INPUT_DEVICE_TABLET_TOOL:
		setup_pen(device);
		break;
	default:
		assert(false && "tried to add non-tablet as tablet");
	}
}
