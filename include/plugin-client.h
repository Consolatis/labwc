/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PLUGIN_CLIENT_H
#define LABWC_PLUGIN_CLIENT_H

#include <stdbool.h>
#include <wayland-util.h>

#define LABWC_PLUGIN_API_VERSION 1

struct view;
struct labwc_plugin;

enum labwc_plugin_capabilities {
	LABWC_CAP_NONE       = 0,
	LABWC_CAP_VIEW_ALIGN = 1 << 0,

	LABWC_CAP_COUNT
};

struct labwc {
	uint8_t api_version;
	/* Pad to 4 byte */
	uint8_t reserved[3];

	/* generic plugin framework functions */
	bool (*plugin_register)(struct labwc_plugin *plugin);
	void (*plugin_unregister)(struct labwc_plugin *plugin);

	/* functions provided by the labwc API */
	int (*get_view_count)(void);
};

struct labwc_plugin_hooks {
	void (*view_title_changed)(const struct view *view, const char *title);
};

struct labwc_plugin {
	uint8_t api_version;
	/* Pad to 4 byte */
	uint8_t reserved[3];

	const char *name;
	const char *description;
	uint32_t capabilities; /* enum labwc_plugin_capabilities */

	/* TODO: somehow prevent leaking this into the plugin */
	struct wl_list labwc_link;

	struct {
		void (*init)(const struct labwc *labwc);
		void (*reconfigure)(const struct labwc *labwc);
		void (*finish)(const struct labwc *labwc);
	} on;

	struct labwc_plugin_hooks hooks;
};

#endif /* LABWC_PLUGIN_CLIENT_H */
