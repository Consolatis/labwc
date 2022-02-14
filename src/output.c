// SPDX-License-Identifier: GPL-2.0-only
/*
 * output.c: labwc output and rendering
 *
 * Copyright (C) 2019-2021 Johan Malm
 * Copyright (C) 2020 The Sway authors
 */

#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include <assert.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/util/region.h>
#include <wlr/util/log.h>
#include "buffer.h"
#include "labwc.h"
#include "layers.h"
#include "menu/menu.h"
#include "ssd.h"
#include "theme.h"

static void
output_frame_notify(struct wl_listener *listener, void *data)
{
	struct output *output = wl_container_of(listener, output, frame);

	if (!output->wlr_output->enabled) {
		return;
	}

	wlr_scene_output_commit(output->scene_output);

	struct timespec now = { 0 };
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(output->scene_output, &now);
}

static void
output_destroy_notify(struct wl_listener *listener, void *data)
{
	struct output *output = wl_container_of(listener, output, destroy);
	wl_list_remove(&output->link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->destroy.link);
}

static void
new_output_notify(struct wl_listener *listener, void *data)
{
	/*
	 * This event is rasied by the backend when a new output (aka display
	 * or monitor) becomes available.
	 */
	struct server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/*
	 * Configures the output created by the backend to use our allocator
	 * and our renderer. Must be done once, before commiting the output
	 */
	if (!wlr_output_init_render(wlr_output, server->allocator,
			server->renderer)) {
		wlr_log(WLR_ERROR, "unable to init output renderer");
		return;
	}

	wlr_log(WLR_DEBUG, "enable output");
	wlr_output_enable(wlr_output, true);

	/* The mode is a tuple of (width, height, refresh rate). */
	wlr_log(WLR_DEBUG, "set preferred mode");
	struct wlr_output_mode *preferred_mode =
		wlr_output_preferred_mode(wlr_output);
	wlr_output_set_mode(wlr_output, preferred_mode);

	/*
	 * Sometimes the preferred mode is not available due to hardware
	 * constraints (e.g. GPU or cable bandwidth limitations). In these
	 * cases it's better to fallback to lower modes than to end up with
	 * a black screen. See sway@4cdc4ac6
	 */
	if (!wlr_output_test(wlr_output)) {
		wlr_log(WLR_DEBUG,
			"preferred mode rejected, falling back to another mode");
		struct wlr_output_mode *mode;
		wl_list_for_each(mode, &wlr_output->modes, link) {
			if (mode == preferred_mode) {
				continue;
			}
			wlr_output_set_mode(wlr_output, mode);
			if (wlr_output_test(wlr_output)) {
				break;
			}
		}
	}

	wlr_output_commit(wlr_output);

	struct output *output = calloc(1, sizeof(struct output));
	output->wlr_output = wlr_output;
	wlr_output->data = output;
	output->server = server;
	wlr_output_effective_resolution(wlr_output,
		&output->usable_area.width, &output->usable_area.height);
	wl_list_insert(&server->outputs, &output->link);

	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	wl_list_init(&output->layers[0]);
	wl_list_init(&output->layers[1]);
	wl_list_init(&output->layers[2]);
	wl_list_init(&output->layers[3]);
	/*
	 * Arrange outputs from left-to-right in the order they appear.
	 * TODO: support configuration in run-time
	 */

	if (rc.adaptive_sync) {
		wlr_log(WLR_INFO, "enable adaptive sync on %s",
			wlr_output->name);
		wlr_output_enable_adaptive_sync(wlr_output, true);
	}

	wlr_output_layout_add_auto(server->output_layout, wlr_output);

	/* TODO: check this makes sense */
	struct wlr_scene_output *scene_output;
	wl_list_for_each (scene_output, &output->server->scene->outputs, link) {
		if (scene_output->output == wlr_output) {
			output->scene_output = scene_output;
			break;
		}
	}
	assert(output->scene_output);
}

void
output_init(struct server *server)
{
	server->new_output.notify = new_output_notify;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	/*
	 * Create an output layout, which is a wlroots utility for working with
	 * an arrangement of screens in a physical layout.
	 */
	server->output_layout = wlr_output_layout_create();
	if (!server->output_layout) {
		wlr_log(WLR_ERROR, "unable to create output layout");
		exit(EXIT_FAILURE);
	}

	/* Enable screen recording with wf-recorder */
	wlr_xdg_output_manager_v1_create(server->wl_display,
		server->output_layout);

	wl_list_init(&server->outputs);

	output_manager_init(server);
}

static void
output_update_for_layout_change(struct server *server)
{
	/* Adjust window positions/sizes */
	struct view *view;
	wl_list_for_each(view, &server->views, link) {
		view_adjust_for_layout_change(view);
	}

	/*
	 * "Move" each wlr_output_cursor (in per-output coordinates) to
	 * align with the seat cursor.  Set a default cursor image so
	 * that the cursor isn't invisible on new outputs.
	 *
	 * TODO: remember the most recent cursor image (see cursor.c)
	 * and set that rather than XCURSOR_DEFAULT
	 */
	wlr_cursor_move(server->seat.cursor, NULL, 0, 0);
	wlr_xcursor_manager_set_cursor_image(server->seat.xcursor_manager,
		XCURSOR_DEFAULT, server->seat.cursor);

	/* Redraw everything */
	damage_all_outputs(server);
}

static void
output_config_apply(struct server *server,
		struct wlr_output_configuration_v1 *config)
{
	server->pending_output_config = config;

	struct wlr_output_configuration_head_v1 *head;
	wl_list_for_each(head, &config->heads, link) {
		struct wlr_output *o = head->state.output;
		bool need_to_add = head->state.enabled && !o->enabled;
		if (need_to_add) {
			wlr_output_layout_add_auto(server->output_layout, o);
		}

		bool need_to_remove = !head->state.enabled && o->enabled;
		if (need_to_remove) {
			/* TODO: should we output->scene_output = NULL; ?? */
			wlr_output_layout_remove(server->output_layout, o);
		}

		wlr_output_enable(o, head->state.enabled);
		if (head->state.enabled) {
			if (head->state.mode) {
				wlr_output_set_mode(o, head->state.mode);
			} else {
				int32_t width = head->state.custom_mode.width;
				int32_t height = head->state.custom_mode.height;
				int32_t refresh = head->state.custom_mode.refresh;
				wlr_output_set_custom_mode(o, width,
					height, refresh);
			}
			wlr_output_layout_move(server->output_layout, o,
				head->state.x, head->state.y);
			wlr_output_set_scale(o, head->state.scale);
			wlr_output_set_transform(o, head->state.transform);
		}
		wlr_output_commit(o);
	}

	server->pending_output_config = NULL;
	output_update_for_layout_change(server);
}

static bool
verify_output_config_v1(const struct wlr_output_configuration_v1 *config)
{
	/* TODO implement */
	return true;
}

static void
handle_output_manager_apply(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, output_manager_apply);
	struct wlr_output_configuration_v1 *config = data;

	bool config_is_good = verify_output_config_v1(config);

	if (config_is_good) {
		output_config_apply(server, config);
		wlr_output_configuration_v1_send_succeeded(config);
	} else {
		wlr_output_configuration_v1_send_failed(config);
	}
	wlr_output_configuration_v1_destroy(config);
	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		wlr_xcursor_manager_load(server->seat.xcursor_manager,
			output->wlr_output->scale);
	}
}

/*
 * Take the way outputs are currently configured/layed out and turn that into
 * a struct that we send to clients via the wlr_output_configuration v1
 * interface
 */
static struct
wlr_output_configuration_v1 *create_output_config(struct server *server)
{
	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();
	if (!config) {
		wlr_log(WLR_ERROR, "wlr_output_configuration_v1_create()");
		return NULL;
	}

	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct wlr_output_configuration_head_v1 *head =
			wlr_output_configuration_head_v1_create(config,
				output->wlr_output);
		if (!head) {
			wlr_log(WLR_ERROR,
				"wlr_output_configuration_head_v1_create()");
			wlr_output_configuration_v1_destroy(config);
			return NULL;
		}
		struct wlr_box box;
		wlr_output_layout_get_box(server->output_layout,
			output->wlr_output, &box);
		if (!wlr_box_empty(&box)) {
			head->state.x = box.x;
			head->state.y = box.y;
		} else {
			wlr_log(WLR_ERROR, "failed to get output layout box");
		}
	}
	return config;
}

static void
handle_output_layout_change(struct wl_listener *listener, void *data)
{
	struct server *server =
		wl_container_of(listener, server, output_layout_change);

	bool done_changing = server->pending_output_config == NULL;
	if (done_changing) {
		struct wlr_output_configuration_v1 *config =
			create_output_config(server);
		if (config) {
			wlr_output_manager_v1_set_configuration(
				server->output_manager, config);
		} else {
			wlr_log(WLR_ERROR,
				"wlr_output_manager_v1_set_configuration()");
		}
		struct output *output;
		wl_list_for_each(output, &server->outputs, link) {
			if (output) {
				arrange_layers(output);
			}
		}
		output_update_for_layout_change(server);
	}
}

void
output_manager_init(struct server *server)
{
	server->output_manager = wlr_output_manager_v1_create(server->wl_display);

	server->output_layout_change.notify = handle_output_layout_change;
	wl_signal_add(&server->output_layout->events.change,
		&server->output_layout_change);

	server->output_manager_apply.notify = handle_output_manager_apply;
	wl_signal_add(&server->output_manager->events.apply,
		&server->output_manager_apply);
}

struct output *
output_from_wlr_output(struct server *server, struct wlr_output *wlr_output)
{
	struct output *output;
	wl_list_for_each (output, &server->outputs, link) {
		if (output->wlr_output == wlr_output) {
			return output;
		}
	}
	return NULL;
}

struct wlr_box
output_usable_area_in_layout_coords(struct output *output)
{
	struct wlr_box box = output->usable_area;
	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(output->server->output_layout,
		output->wlr_output, &ox, &oy);
	box.x -= ox;
	box.y -= oy;
	return box;
}

struct wlr_box
output_usable_area_from_cursor_coords(struct server *server)
{
	struct wlr_output *wlr_output;
	wlr_output = wlr_output_layout_output_at(server->output_layout,
		server->seat.cursor->x, server->seat.cursor->y);
	struct output *output = output_from_wlr_output(server, wlr_output);
	return output_usable_area_in_layout_coords(output);
}
