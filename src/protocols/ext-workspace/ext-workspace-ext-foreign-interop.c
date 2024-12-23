// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include "protocols/ext-workspace.h"
#include "protocols/transaction-addon.h"
#include "protocols/workspace_interop.h"
#include "interop-protocol.h"
#include "ext-foreign-toplevel-list-v1-protocol.h"

#define INTEROP_VERSION 1

struct interop_mapping {
	struct interop_handle *handle;
	struct lab_ext_workspace *workspace;
	struct {
		struct wl_listener workspace_destroy;
	} on;
	struct wl_list link;
};

static void
send_event(struct interop_mapping *mapping, struct wl_resource *handle_resource,
		void (*fn)(struct wl_resource *handle_res, struct wl_resource *ws_res))
{
	struct lab_wl_resource_addon *handle_addon, *ws_addon;
	handle_addon = wl_resource_get_user_data(handle_resource);

	struct wl_resource *workspace_res, *toplevel_res;
	wl_resource_for_each(workspace_res, &mapping->workspace->resources) {
		ws_addon = wl_resource_get_user_data(workspace_res);
		if (ws_addon->ctx != handle_addon->ctx) {
			continue;
		}
		fn(handle_resource, workspace_res);
		//toplevel_update_idle_source(mapping->handle->toplevel);
		wl_resource_for_each(toplevel_res, &mapping->handle->toplevel->resources) {
			ext_foreign_toplevel_handle_v1_send_done(toplevel_res);
		}
		break;
	}
}

static void
broadcast_event(struct interop_mapping *mapping,
		void (*fn)(struct wl_resource *handle_res, struct wl_resource *ws_res))
{
	struct wl_resource *handle_resource;
	wl_resource_for_each(handle_resource, &mapping->handle->resources) {
		send_event(mapping, handle_resource, fn);
	}
}

static void
mapping_destroy(struct interop_mapping *mapping)
{
	broadcast_event(mapping, interop_handle_send_workspace_leave);
	wl_list_remove(&mapping->on.workspace_destroy.link);
	wl_list_remove(&mapping->link);
	free(mapping);
}

static void
mapping_handle_workspace_destroy(struct wl_listener *listener, void *data)
{
	struct interop_mapping *mapping =
		wl_container_of(listener, mapping, on.workspace_destroy);
	mapping_destroy(mapping);
}

static struct interop_mapping *
mapping_create(struct interop_handle *handle, struct lab_ext_workspace *workspace)
{
	struct interop_mapping *mapping = calloc(1, sizeof(*mapping));
	mapping->handle = handle;
	mapping->workspace = workspace;
	mapping->on.workspace_destroy.notify = mapping_handle_workspace_destroy;
	wl_signal_add(&workspace->events.destroy, &mapping->on.workspace_destroy);
	wl_list_insert(&handle->mappings, &mapping->link);
	broadcast_event(mapping, interop_handle_send_workspace_enter);
	return mapping;
}

static struct interop_mapping *
find_mapping(struct interop_handle *handle, struct lab_ext_workspace *workspace)
{
	struct interop_mapping *mapping;
	wl_list_for_each(mapping, &handle->mappings, link) {
		if (mapping->workspace == workspace) {
			return mapping;
		}
	}
	return NULL;
}

static void
interop_handle_destroy(struct interop_handle *handle)
{
	struct interop_mapping *mapping, *mapping_tmp;
	wl_list_for_each_safe(mapping, mapping_tmp, &handle->mappings, link) {
		mapping_destroy(mapping);
	}
	assert(wl_list_empty(&handle->mappings));

	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &handle->resources) {
		//interop_handle_destroyed(resource);
		wl_resource_destroy(resource);
	}
	assert(wl_list_empty(&handle->resources));

	wl_list_remove(&handle->on.toplevel_destroy.link);
	wl_list_remove(&handle->link);
	free(handle);
}

static void
handle_handle_toplevel_destroy(struct wl_listener *listener, void *data)
{
	struct interop_handle *handle =
		wl_container_of(listener, handle, on.toplevel_destroy);
	interop_handle_destroy(handle);
}

// client side api
static void
handle_toplevel_enter_workspace(struct wl_client *client,
		struct wl_resource *handle_resource,
		struct wl_resource *workspace_resource)
{
	struct lab_wl_resource_addon *ws_addon, *handle_addon;
	ws_addon = wl_resource_get_user_data(workspace_resource);
	if (!ws_addon) {
		return;
	}
	handle_addon = wl_resource_get_user_data(handle_resource);
	if (!handle_addon) {
		return;
	}
	struct interop_handle *handle = handle_addon->data;
	wl_signal_emit_mutable(&handle->events.request_join_workspace, ws_addon->data);
}

static void
handle_toplevel_leave_workspace(struct wl_client *client,
		struct wl_resource *handle_resource,
		struct wl_resource *workspace_resource)
{
	struct lab_wl_resource_addon *ws_addon, *handle_addon;
	ws_addon = wl_resource_get_user_data(workspace_resource);
	if (!ws_addon) {
		return;
	}
	handle_addon = wl_resource_get_user_data(handle_resource);
	if (!handle_addon) {
		return;
	}
	struct interop_handle *handle = handle_addon->data;
	wl_signal_emit_mutable(&handle->events.request_leave_workspace, ws_addon->data);
}

static const struct interop_handle_interface interop_handle_impl = {
	.enter_workspace = handle_toplevel_enter_workspace,
	.leave_workspace = handle_toplevel_leave_workspace,
};

static void
handle_instance_resource_destroy(struct wl_resource *resource)
{
	struct lab_wl_resource_addon *addon = wl_resource_get_user_data(resource);
	if (addon) {
		lab_resource_addon_destroy(addon);
		wl_resource_set_user_data(resource, NULL);
	}
	wl_list_remove(wl_resource_get_link(resource));
}

static void
manager_handle_create_handle(struct wl_client *client,
		struct wl_resource *manager_res,
		struct wl_resource *toplevel_res,
		struct wl_resource *workspace_manager_res, uint32_t obj_id)
{
	struct lab_wl_resource_addon *ws_addon, *handle_addon;
	struct interop_manager *manager = wl_resource_get_user_data(manager_res);
	if (!manager) {
		return;
	}

	ws_addon = wl_resource_get_user_data(workspace_manager_res);
	if (!ws_addon) {
		//FIXME: what to do, complain loudly? send error? send error.
		return;
	}

	//struct wlr_foreign_toplevel_handle_v1 *toplevel =
	struct wlr_ext_foreign_toplevel_handle_v1 *toplevel =
		wl_resource_get_user_data(toplevel_res);
	if (!toplevel) {
		//FIXME: what to do, complain loudly? send error? send error.
		return;
	}

	struct interop_handle *handle = NULL;
	struct interop_handle *handle_tmp;
	wl_list_for_each(handle_tmp, &manager->handles, link) {
		if (handle_tmp->toplevel == toplevel) {
			handle = handle_tmp;
			break;
		}
	}
	if (!handle) {
		//FIXME: what to do, complain loudly? send error? send error.
		return;
	}

	wlr_log(WLR_INFO, "Creating handle resource");
	struct wl_resource *handle_resource = wl_resource_create(client,
			&interop_handle_interface,
			wl_resource_get_version(manager_res), obj_id);
	if (!handle_resource) {
		wl_client_post_no_memory(client);
		return;
	}

	handle_addon = lab_resource_addon_create(ws_addon->ctx);
	handle_addon->data = handle;

	wl_resource_set_implementation(handle_resource, &interop_handle_impl,
		handle_addon, handle_instance_resource_destroy);

	wl_list_insert(&handle->resources, wl_resource_get_link(handle_resource));

	// initial sync
	struct interop_mapping *mapping;
	wl_list_for_each(mapping, &handle->mappings, link) {
		send_event(mapping, handle_resource, interop_handle_send_workspace_enter);
	}
}


static const struct interop_manager_interface manager_impl = {
	.create_handle = manager_handle_create_handle,
};

static void
manager_instance_resource_destroy(struct wl_resource *resource)
{
	wl_list_remove(wl_resource_get_link(resource));
}

static void
manager_handle_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id)
{
	struct interop_manager *manager = data;
	struct wl_resource *manager_resource = wl_resource_create(client,
			&interop_manager_interface,
			version, id);
	if (!manager_resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(manager_resource, &manager_impl,
		manager, manager_instance_resource_destroy);

	wl_list_insert(&manager->resources, wl_resource_get_link(manager_resource));
}

static void
manager_handle_display_destroy(struct wl_listener *listener, void *data)
{
	struct interop_manager *manager =
		wl_container_of(listener, manager, on.display_destroy);

	struct interop_handle *handle, *tmp;
	wl_list_for_each_safe(handle, tmp, &manager->handles, link) {
		interop_handle_destroy(handle);
	}

	wl_list_remove(&manager->on.display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

/* Public API */

struct interop_manager *
interop_manager_create(struct wl_display *display, uint32_t version)
{
	assert(version <= INTEROP_VERSION);

	struct interop_manager *manager = calloc(1, sizeof(*manager));
	manager->global = wl_global_create(display,
			&interop_manager_interface,
			version, manager, manager_handle_bind);

	if (!manager->global) {
		free(manager);
		return NULL;
	}

	manager->on.display_destroy.notify = manager_handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->on.display_destroy);

	wl_list_init(&manager->handles);
	wl_list_init(&manager->resources);
	return manager;
}

struct interop_handle *
interop_handle_create(struct interop_manager *manager,
		struct wlr_ext_foreign_toplevel_handle_v1 *toplevel)
//		struct wlr_foreign_toplevel_handle_v1 *toplevel)
{
	struct interop_handle *handle = calloc(1, sizeof(*handle));
	handle->manager = manager;
	handle->toplevel = toplevel;
	handle->on.toplevel_destroy.notify = handle_handle_toplevel_destroy;
	wl_signal_add(&toplevel->events.destroy, &handle->on.toplevel_destroy);
	wl_signal_init(&handle->events.request_join_workspace);
	wl_signal_init(&handle->events.request_leave_workspace);
	wl_list_init(&handle->mappings);
	wl_list_init(&handle->resources);
	wl_list_insert(&manager->handles, &handle->link);
	return handle;
}

void
toplevel_join_workspace(struct interop_handle *handle,
		struct lab_ext_workspace *workspace)
{
	if (find_mapping(handle, workspace)) {
		return;
	}
	mapping_create(handle, workspace);
}

void
toplevel_leave_workspace(struct interop_handle *handle,
		struct lab_ext_workspace *workspace)
{
	struct interop_mapping *mapping = find_mapping(handle, workspace);
	if (!mapping) {
		return;
	}
	mapping_destroy(mapping);
}
