#include <assert.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include <wayland-server.h>
#include "ui/tabs.h"
#include "ui.h"
#include "node.h"
#include "common/mem.h"

#if 0
TODO: implement
struct tab_buttons {
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *background;
};
#endif

/* internal helpers */

static void
tab_view_select(struct tab_view *self, struct wlr_scene_node *node)
{
	assert(!node || tab_view_contains(self, node));

	// TODO: button yadda yadda

	if (self->selected) {
		wlr_scene_node_set_enabled(self->selected, false);
	}

	if (node) {
		wlr_scene_node_set_enabled(node, true);
	}

	struct wlr_scene_node *old = self->selected;
	self->selected = node;

	wl_signal_emit_mutable(&self->events.selection, old);
}

/* internal callbacks */

static void
handle_tabs_axis(struct ui_element *header, struct ui_context *ctx)
{
	// replace with wl_signal_emit_mutable() ?
	wlr_log(WLR_ERROR, "Should switch tabs");
	struct tab_view *self = header->data;
	tab_view_cycle(self);
}

/* public api */

struct tab_view *
tab_view_create(struct wlr_scene_tree *parent, int tab_height)
{
	struct tab_view *tab_view = znew(*tab_view);
	tab_view->tab_height = tab_height;
	tab_view->header.tree = wlr_scene_tree_create(parent);

	tab_view->header.background = wlr_scene_rect_create(
		tab_view->header.tree, 0, tab_height,
		(float[4]){0, 1, 0, 1});

	tab_view->content.background = wlr_scene_rect_create(
		tab_view->header.tree, 0, 0,
		(float[4]){0, 0, 1, 1});

	/* Attach UI element */

	// FIXME: mem leak, handle header->tree->node destroy and free(tabs)
	struct ui_element *tabs = znew(*tabs);
	tabs->impl[UI_MOUSE_AXIS] = handle_tabs_axis;
	tabs->data = tab_view;
	node_descriptor_create(&tab_view->header.tree->node,
		LAB_NODE_DESC_UI_ELEMENT, tabs);

	tab_view->content.tree = wlr_scene_tree_create(parent);
	wlr_scene_node_set_position(&tab_view->content.tree->node, 0, tab_height);

	wl_signal_init(&tab_view->events.selection);

	return tab_view;
}

void
tab_view_configure(struct tab_view *tab_view, struct wlr_box geo)
{
	//geo.height -= tab_view->tab_height;
	wlr_scene_rect_set_size(tab_view->header.background, geo.width, tab_view->tab_height);
	wlr_scene_rect_set_size(tab_view->content.background, geo.width, geo.height);
}

void
tab_view_add(struct tab_view *self, struct wlr_scene_node *node)
{
	assert(!tab_view_contains(self, node));

	// TODO: button yadda yadda
	wlr_scene_node_reparent(node, self->content.tree);
	if (!self->selected) {
		tab_view_select(self, node);
	} else {
		wlr_scene_node_set_enabled(node, false);
	}
}

bool
tab_view_contains(struct tab_view *self, struct wlr_scene_node *node)
{
	return node->parent == self->content.tree;
}

void
tab_view_remove(struct tab_view *self, struct wlr_scene_node *node,
		struct wlr_scene_tree *new_parent)
{
	assert(tab_view_contains(self, node));

	// TODO: button yadda yadda
	if (self->selected == node && !tab_view_cycle(self)) {
		tab_view_select(self, NULL);
	}
	wlr_scene_node_reparent(node, new_parent);
	wlr_scene_node_set_enabled(node, true);
}

bool
tab_view_cycle(struct tab_view *self)
{
	if (!self->selected || wl_list_length(&self->content.tree->children) <= 1) {
		return false;
	}

	struct wl_list *next_link = self->selected->link.next;
	if (next_link == &self->content.tree->children) {
		next_link = next_link->next;
	}
	assert(next_link != &self->selected->link);

	struct wlr_scene_node *node = wl_container_of(next_link, node, link);
	tab_view_select(self, node);
	return true;
}
