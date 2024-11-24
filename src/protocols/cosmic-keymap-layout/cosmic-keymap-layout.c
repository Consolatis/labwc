// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_seat.h>
#include "common/mem.h"
#include "common/list.h"
#include "protocols/cosmic-keymap-layout.h"
#include "cosmic-keymap-unstable-v1-protocol.h"

#define LAB_COSMIC_KEYMAP_LAYOUT_VERSION 1

struct keymap_listener {
	struct lab_cosmic_keymap_layout_manager *manager;
	struct wlr_keyboard *kb;
	uint32_t group;
	struct {
		struct wl_listener modifiers;
		struct wl_listener destroy;
	} on_kb;
	struct wl_list link;
	struct wl_list keymap_resources;
};

/* Keymap listeners */
static void
keymap_listener_destroy(struct keymap_listener *listener)
{
	wl_list_remove(&listener->on_kb.modifiers.link);
	wl_list_remove(&listener->on_kb.destroy.link);
	wl_list_remove(&listener->link);

	struct wl_resource *res, *tmp;
	wl_resource_for_each_safe(res, tmp, &listener->keymap_resources) {
		wl_resource_set_user_data(res, NULL);
		wl_resource_destroy(res);
	}
	free(listener);
}

static void
kb_handle_destroy(struct wl_listener *listener, void *data)
{
	struct keymap_listener *keymap_listener =
		wl_container_of(listener, keymap_listener, on_kb.destroy);

	keymap_listener_destroy(keymap_listener);
}

static void
kb_handle_modifier(struct wl_listener *listener, void *data)
{
	struct keymap_listener *keymap_listener =
		wl_container_of(listener, keymap_listener, on_kb.modifiers);
	uint32_t group = keymap_listener->kb->modifiers.group;

	//FIXME: something weird going on here, on a non-group modifier
	//       press modifiers.group is reset to 0 for whatever reason
	//       likely related to running nested and the external wayland
	//       modifier events just getting passed through.

	if (keymap_listener->group == group) {
		return;
	}
	keymap_listener->group = group;

	struct wl_resource *res;
	wl_resource_for_each(res, &keymap_listener->keymap_resources) {
		zcosmic_keymap_v1_send_group(res, group);
	}
}

static struct keymap_listener *
keymap_listener_create(struct lab_cosmic_keymap_layout_manager *manager, struct wlr_keyboard *kb)
{
	struct keymap_listener *listener = znew(*listener);
	listener->kb = kb;
	listener->manager = manager;
	listener->group = kb->modifiers.group;
	wl_list_init(&listener->keymap_resources);

	listener->on_kb.modifiers.notify = kb_handle_modifier;
	wl_signal_add(&kb->events.modifiers, &listener->on_kb.modifiers);

	listener->on_kb.destroy.notify = kb_handle_destroy;
	wl_signal_add(&kb->base.events.destroy, &listener->on_kb.destroy);

	wl_list_append(&manager->keymap_listeners, &listener->link);

	return listener;
}


/* Keymap handlers */
static void
keymap_handle_set_group(struct wl_client *client, struct wl_resource *resource, uint32_t group)
{
	struct keymap_listener *listener = wl_resource_get_user_data(resource);
	assert(listener);

	wl_signal_emit_mutable(&listener->manager->events.request_layout, &group);
}

static void
keymap_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct zcosmic_keymap_v1_interface keymap_impl = {
	.set_group = keymap_handle_set_group,
	.destroy = keymap_handle_destroy,
};

static void
keymap_instance_resource_destroy(struct wl_resource *resource)
{
	wl_list_remove(wl_resource_get_link(resource));

	struct keymap_listener *listener = wl_resource_get_user_data(resource);
	if (listener && wl_list_empty(&listener->keymap_resources)) {
		keymap_listener_destroy(listener);
	}
}

/* Manager handlers */
static void
manager_handle_get_keymap(struct wl_client *client, struct wl_resource *resource,
		uint32_t keymap, struct wl_resource *keyboard)
{
	struct lab_cosmic_keymap_layout_manager *manager =
		wl_resource_get_user_data(resource);
	assert(manager);

	struct wlr_seat_client *seat_client = wl_resource_get_user_data(keyboard);
	if (!seat_client) {
		wlr_log(WLR_ERROR, "failed to find seat_client");
		return;
	}
	struct wlr_keyboard *kb = seat_client->seat->keyboard_state.keyboard;
	if (!kb) {
		wlr_log(WLR_ERROR, "failed to find keyboard");
		return;
	}
	uint32_t version = wl_resource_get_version(resource);
	struct wl_resource *keymap_resource =
		wl_resource_create(client, &zcosmic_keymap_v1_interface, version, keymap);
	if (!resource) {
		wlr_log(WLR_ERROR, "failed to create keymap resource");
		wl_client_post_no_memory(client);
		return;
	}

	struct keymap_listener *listener = NULL;

	struct keymap_listener *kl;
	wl_list_for_each(kl, &manager->keymap_listeners, link) {
		if (kl->kb == kb) {
			listener = kl;
			break;
		}
	}
	if (!listener) {
		listener = keymap_listener_create(manager, kb);
	}

	wl_resource_set_implementation(keymap_resource, &keymap_impl, listener,
		keymap_instance_resource_destroy);

	wl_list_insert(&listener->keymap_resources, wl_resource_get_link(keymap_resource));

	zcosmic_keymap_v1_send_group(keymap_resource, listener->group);
}

static void
manager_handle_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct zcosmic_keymap_manager_v1_interface manager_impl = {
	.get_keymap = manager_handle_get_keymap,
	.destroy = manager_handle_destroy,
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
	struct lab_cosmic_keymap_layout_manager *manager = data;
	struct wl_resource *resource = wl_resource_create(client,
			&zcosmic_keymap_manager_v1_interface,
			version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &manager_impl,
		manager, manager_instance_resource_destroy);

	wl_list_insert(&manager->manager_resources, wl_resource_get_link(resource));
}

static void
manager_handle_display_destroy(struct wl_listener *listener, void *data)
{
	struct lab_cosmic_keymap_layout_manager *manager =
		wl_container_of(listener, manager, on.display_destroy);

	wl_global_destroy(manager->global);

	struct keymap_listener *kl, *kl_tmp;
	wl_list_for_each_safe(kl, kl_tmp, &manager->keymap_listeners, link) {
		keymap_listener_destroy(kl);
	}

	struct wl_resource *resource, *res_tmp;
	wl_resource_for_each_safe(resource, res_tmp, &manager->manager_resources) {
		wl_resource_destroy(resource);
	}

	wl_list_remove(&manager->on.display_destroy.link);
	free(manager);
}

/* Public API */
struct lab_cosmic_keymap_layout_manager *
lab_cosmic_keymap_layout_manager_create(struct wl_display *display, uint32_t version)
{
	assert(version <= LAB_COSMIC_KEYMAP_LAYOUT_VERSION);
	struct lab_cosmic_keymap_layout_manager *manager = znew(*manager);

	manager->global = wl_global_create(display,
		&zcosmic_keymap_manager_v1_interface,
		version, manager, manager_handle_bind);

	if (!manager->global) {
		free(manager);
		return NULL;
	}

	manager->on.display_destroy.notify = manager_handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->on.display_destroy);

	wl_signal_init(&manager->events.request_layout);
	wl_list_init(&manager->manager_resources);
	wl_list_init(&manager->keymap_listeners);
	return manager;
}
