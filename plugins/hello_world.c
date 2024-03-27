#include <stdio.h>
#include "../include/plugin-client.h"

static struct labwc_plugin self;

static void
plugin_on_init(const struct labwc *labwc)
{
	fprintf(stderr, "[%s] loaded\n", self.name);
	fprintf(stderr, "[%s] Number of open views: %d\n",
		self.name, labwc->get_view_count());
}

static void
plugin_on_reconfigure(const struct labwc *labwc)
{
	fprintf(stderr, "[%s] labwc reconfigured\n", self.name);
}

static void
plugin_on_finish(const struct labwc *labwc)
{
	fprintf(stderr, "[%s] finished\n", self.name);
}

static void
on_view_title_changed(const struct view *view, const char *title)
{
	fprintf(stderr, "[%s] title for %p changed to %s\n", self.name, view, title);
}

static struct labwc_plugin self = {
	.api_version = LABWC_PLUGIN_API_VERSION,
	.name = "Hello World",
	.on = {
		.init = plugin_on_init,
		.reconfigure = plugin_on_reconfigure,
		.finish = plugin_on_finish
	},
	.hooks = {
		.view_title_changed = on_view_title_changed
	}
};

void
module_init(const struct labwc *labwc)
{
	labwc->plugin_register(&self);
}

void
module_finish(void)
{
}
