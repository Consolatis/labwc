/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_UI_TABS_H
#define LABWC_UI_TABS_H

#include <stdbool.h>

struct wlr_scene_tree;
struct wlr_scene_rect;
struct wlr_scene_node;

struct tab_view {
	struct {
		struct wlr_scene_tree *tree;
		struct wlr_scene_rect *background;
	} header;
	struct {
		struct wlr_scene_tree *tree;
		struct wlr_scene_rect *background;
	} content;
	uint16_t tab_height;
	struct wlr_scene_node *selected;

	struct {
		struct wl_signal selection;
	} events;
};

struct tab_view *tab_view_create(struct wlr_scene_tree *parent, int tab_height);
void tab_view_configure(struct tab_view *tab_view, struct wlr_box geo);
void tab_view_add(struct tab_view *self, struct wlr_scene_node *node);
bool tab_view_contains(struct tab_view *self, struct wlr_scene_node *node);
void tab_view_remove(struct tab_view *self, struct wlr_scene_node *node,
	struct wlr_scene_tree *new_parent);
bool tab_view_cycle(struct tab_view *self);


#endif /* LABWC_UI_TABS_H */
