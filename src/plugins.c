#include <assert.h>
#include <dlfcn.h>
#include <wlr/util/log.h>
#include "common/list.h"
#include "common/mem.h"
#include "labwc.h"
#include "plugin-client.h"
#include "plugins.h"
#include "view.h"

/*
 * These ensure that we don't try to access
 * undefined memory in the module supplied
 * plugin struct.
 */
#define HOOK_SUPPORTED(plugin, hook) ((plugin)->api_version >= (HOOK_ ## hook ## _SINCE_VERSION))
#define HOOK_VIEW_TITLE_CHANGE_SINCE_VERSION 1

struct _handle {
	void *handle;
	struct wl_list link;    /* handles */

	/* module wide init and finish functions */
	void (*init)(const struct labwc *labwc);
	void (*finish)(const struct labwc *labwc);
};

static struct wl_list handles;
static struct wl_list plugins; /* plugin.labwc_link */

static const struct labwc labwc;      /* forward declare */
static const struct server *g_server; /* set by plugins_init() */

static bool
plugin_register(struct labwc_plugin *plugin)
{
	wlr_log(WLR_ERROR, "plugin %s registering", plugin->name);
	if (!plugin->on.init) {
		wlr_log(WLR_ERROR, "plugin %s does not provide a init function", plugin->name);
		return false;
	}
	wl_list_append(&plugins, &plugin->labwc_link);
	wlr_log(WLR_ERROR, "initializing plugin %s", plugin->name);
	plugin->on.init(&labwc);
	return true;
}

static void
plugin_finish(struct labwc_plugin *plugin) {
	wlr_log(WLR_ERROR, "finishing plugin %s", plugin->name);
	if (plugin->on.finish) {
		plugin->on.finish(&labwc);
	}
	wl_list_remove(&plugin->labwc_link);
}

static void
plugin_unregister(struct labwc_plugin *plugin)
{
	struct labwc_plugin *_plugin, *tmp;
	wl_list_for_each_safe(_plugin, tmp, &plugins, labwc_link) {
		if (_plugin == plugin) {
			plugin_finish(plugin);
			return;
		}
	}

	wlr_log(WLR_ERROR, "Plugin %s does not exist", plugin->name);
}

static int
get_view_count(void) {
	return wl_list_length(&g_server->views);
}

static const struct labwc labwc = {
	.api_version = 0,
	.plugin_register = plugin_register,
	.plugin_unregister = plugin_unregister,
	.get_view_count = get_view_count
};

static void *
load_symbol(void *handle, const char *name)
{
	void *symbol = dlsym(handle, name);
	if (!symbol) {
		wlr_log(WLR_ERROR, "Failed to find %s symbol", name);
	}
	return symbol;
}

static bool
load_module(const char *path) {
	assert(path);
	struct _handle *handle = znew(*handle);

	wlr_log(WLR_ERROR, "loading module %s", path);
	handle->handle = dlopen(path, RTLD_LAZY);
	if (!handle->handle) {
		wlr_log_errno(WLR_ERROR, "loading module %s failed", path);
		goto clean_up;
	}

	wlr_log(WLR_ERROR, "initializing module %s", path);
	handle->init = load_symbol(handle->handle, "module_init");
	if (!handle->init) {
		goto clean_up;
	}

	handle->finish = load_symbol(handle->handle, "module_finish");
	if (!handle->finish) {
		goto clean_up;
	}

	wl_list_append(&handles, &handle->link);
	handle->init(&labwc);

	return true;

clean_up:
	zfree(handle);
	return false;
}

void
plugins_init(struct server *server)
{
	g_server = server;
	wl_list_init(&handles);
	wl_list_init(&plugins);
	load_module("/home/user/dev/labwc/plugins/hello_world.so");
}

void
plugins_reconfigure(void)
{
	struct labwc_plugin *plugin;
	wl_list_for_each(plugin, &plugins, labwc_link) {
		if (plugin->on.reconfigure) {
			plugin->on.reconfigure(&labwc);
		}
	}
}

void
plugins_view_title_changed(struct view *view)
{
	const char *title = view_get_string_prop(view, "title");
	struct labwc_plugin *plugin;
	wl_list_for_each(plugin, &plugins, labwc_link) {
		if (HOOK_SUPPORTED(plugin, VIEW_TITLE_CHANGE)
				&& plugin->hooks.view_title_changed) {
			plugin->hooks.view_title_changed(view, title);
		} else if (!HOOK_SUPPORTED(plugin, VIEW_TITLE_CHANGE)) {
			wlr_log(WLR_ERROR,
				"title change hook not supported by plugin api version %d",
				plugin->api_version);
		}
	}
}

void
plugins_finish(void)
{
	struct labwc_plugin *plugin, *plugin_tmp;
	wl_list_for_each_safe(plugin, plugin_tmp, &plugins, labwc_link) {
		plugin_finish(plugin);
	}

	struct _handle *handle, *handle_tmp;
	wl_list_for_each_safe(handle, handle_tmp, &handles, link) {

		wlr_log(WLR_ERROR, "finishing module");
		handle->finish(&labwc);

		wlr_log(WLR_ERROR, "closing handle");
		dlclose(handle->handle);

		wl_list_remove(&handle->link);
		zfree(handle);
	}
}
