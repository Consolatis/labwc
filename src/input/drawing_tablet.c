// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdlib.h>
#include <linux/input-event-codes.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/util/log.h>
#include "common/macros.h"
#include "common/mem.h"
#include "input/cursor.h"
#include "input/drawing_tablet.h"

#include "config/rcxml.h"

static void
setup_pad(struct seat *seat, struct wlr_input_device *wlr_device)
{
	wlr_log(WLR_ERROR, "not setting up pad");
}

static void
handle_axis(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_axis_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;
	if (ev->updated_axes & (WLR_TABLET_TOOL_AXIS_X | WLR_TABLET_TOOL_AXIS_Y)) {
		wlr_log(WLR_DEBUG, "Got new axis pos: %.10f,%.10f", ev->x, ev->y);
		if (ev->x > 0.0f && ev->y > 0.0f) {
			cursor_emulate_move_absolute(tablet->seat, ev->x, ev->y, ev->time_msec);
		} else {
			wlr_log(WLR_INFO, "Blocking strange axis pos: %.10f, %.10f", ev->x, ev->y);
		}
	}
	if (ev->updated_axes & (WLR_TABLET_TOOL_AXIS_TILT_X | WLR_TABLET_TOOL_AXIS_TILT_Y)) {
		wlr_log(WLR_INFO, "Got new axis tilt: %.10f,%1.10f", ev->tilt_x, ev->tilt_y);
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_DISTANCE) {
		wlr_log(WLR_INFO, "Got new axis distance: %.10f", ev->distance);
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_PRESSURE) {
		wlr_log(WLR_INFO, "Got new axis pressure: %.10f", ev->pressure);
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_ROTATION) {
		wlr_log(WLR_INFO, "Got new axis rotation: %.10f", ev->rotation);
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_SLIDER) {
		wlr_log(WLR_INFO, "Got new axis slider: %.10f", ev->slider);
	}
	if (ev->updated_axes & WLR_TABLET_TOOL_AXIS_WHEEL) {
		wlr_log(WLR_INFO, "Got new axis wheel: %.10f", ev->wheel_delta);
	}
	if (!ev->updated_axes) {
		wlr_log(WLR_INFO, "Got empty axis event");
	}
	if (ev->updated_axes > WLR_TABLET_TOOL_AXIS_WHEEL) {
		wlr_log(WLR_ERROR, "Got strange axis event %u: %.10f,%1.10f",
			ev->updated_axes, ev->x, ev->y);
	}
}

static void
handle_proximity(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_proximity_event *ev = data;
	wlr_log(WLR_INFO, "Got proximity event:");
	wlr_log(WLR_INFO, "\t %.10f,%1.10f", ev->x, ev->y);
	wlr_log(WLR_INFO, "\t state %s",
		ev->state == WLR_TABLET_TOOL_PROXIMITY_IN ? "in" : "out");
}

static uint32_t
get_mapped_button(uint32_t src_button)
{
	struct button_map_entry *map_entry;
	for (size_t i = 0; i < rc.tablet.button_map_count; i++) {
		map_entry = &rc.tablet.button_map[i];
		if (map_entry->from == src_button) {
			return map_entry->to;
		}
	}
	return 0;
}

static void
handle_tip(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_tip_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;
	wlr_log(WLR_INFO, "Got tip event:");
	wlr_log(WLR_INFO, "\t %.10f,%1.10f", ev->x, ev->y);
	wlr_log(WLR_INFO, "\t state %s",
		ev->state == WLR_TABLET_TOOL_TIP_UP ? "released" : "pressed");

	uint32_t button = get_mapped_button(BTN_TOOL_PEN);
	if (!button) {
		return;
	}

	cursor_emulate_button(tablet->seat,
		button,
		ev->state == WLR_TABLET_TOOL_TIP_DOWN
			? WLR_BUTTON_PRESSED
			: WLR_BUTTON_RELEASED,
		ev->time_msec);
}

static void
handle_button(struct wl_listener *listener, void *data)
{
	struct wlr_tablet_tool_button_event *ev = data;
	struct drawing_tablet *tablet = ev->tablet->data;
	wlr_log(WLR_INFO, "Got button event: %u %s", ev->button,
		ev->state == WLR_BUTTON_RELEASED ? "release" : "press");

	uint32_t button = get_mapped_button(ev->button);
	if (!button) {
		return;
	}
	cursor_emulate_button(tablet->seat, button, ev->state, ev->time_msec);
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
setup_pen(struct seat *seat, struct wlr_input_device *wlr_device)
{
	wlr_log(WLR_INFO, "setting up tablet");
	struct drawing_tablet *tablet = znew(*tablet);
	tablet->seat = seat;
	tablet->tablet = wlr_tablet_from_input_device(wlr_device);
	tablet->tablet->data = tablet;
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, axis);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, proximity);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, tip);
	CONNECT_SIGNAL(tablet->tablet, &tablet->handlers, button);
	CONNECT_SIGNAL(wlr_device, &tablet->handlers, destroy);
}

void
drawing_tablet_setup_handlers(struct seat *seat, struct wlr_input_device *device)
{
	switch(device->type) {
	case WLR_INPUT_DEVICE_TABLET_PAD:
		setup_pad(seat, device);
		break;
	case WLR_INPUT_DEVICE_TABLET_TOOL:
		setup_pen(seat, device);
		break;
	default:
		assert(false && "tried to add non-tablet as tablet");
	}
}
