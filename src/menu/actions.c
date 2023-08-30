#include <assert.h>
#include "action.h"
#include "labwc.h"
#include "menu.h"
#include "node.h"

/* Internal helpers */
static int
menu_get_full_width(struct menu *menu)
{
	int width = menu->size.width - menu->server->theme->menu_overlap_x;
	int child_width;
	int max_child_width = 0;
	struct menuitem *item;
	wl_list_for_each(item, &menu->menuitems, link) {
		if (!item->submenu) {
			continue;
		}
		child_width = menu_get_full_width(item->submenu);
		if (child_width > max_child_width) {
			max_child_width = child_width;
		}
	}
	return width + max_child_width;
}

static void
menu_configure(struct menu *menu, int lx, int ly, enum menu_align align)
{
	struct theme *theme = menu->server->theme;

	/* Get output local coordinates + output usable area */
	double ox = lx;
	double oy = ly;
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		menu->server->output_layout, lx, ly);
	struct output *output = wlr_output ? output_from_wlr_output(
		menu->server, wlr_output) : NULL;
	if (!output) {
		wlr_log(WLR_ERROR,
			"Failed to position menu %s (%s) and its submenus: "
			"Not enough screen space", menu->id, menu->label);
		return;
	}
	wlr_output_layout_output_coords(menu->server->output_layout,
		wlr_output, &ox, &oy);

	if (align == LAB_MENU_OPEN_AUTO) {
		int full_width = menu_get_full_width(menu);
		if (ox + full_width > output->usable_area.width) {
			align = LAB_MENU_OPEN_LEFT;
		} else {
			align = LAB_MENU_OPEN_RIGHT;
		}
	}

	if (oy + menu->size.height > output->usable_area.height) {
		align &= ~LAB_MENU_OPEN_BOTTOM;
		align |= LAB_MENU_OPEN_TOP;
	} else {
		align &= ~LAB_MENU_OPEN_TOP;
		align |= LAB_MENU_OPEN_BOTTOM;
	}

	if (align & LAB_MENU_OPEN_LEFT) {
		lx -= menu->size.width - theme->menu_overlap_x;
	}
	if (align & LAB_MENU_OPEN_TOP) {
		ly -= menu->size.height;
		if (menu->parent) {
			/* For submenus adjust y to bottom left corner */
			ly += menu->item_height;
		}
	}
	wlr_scene_node_set_position(&menu->scene_tree->node, lx, ly);

	int rel_y;
	int new_lx, new_ly;
	struct menuitem *item;
	wl_list_for_each(item, &menu->menuitems, link) {
		if (!item->submenu) {
			continue;
		}
		if (align & LAB_MENU_OPEN_RIGHT) {
			new_lx = lx + menu->size.width - theme->menu_overlap_x;
		} else {
			new_lx = lx;
		}
		rel_y = item->tree->node.y;
		new_ly = ly + rel_y - theme->menu_overlap_y;
		menu_configure(item->submenu, new_lx, new_ly, align);
	}
}


/* Sets selection (or clears selection if passing NULL) */
static void
menu_set_selection(struct menu *menu, struct menuitem *item)
{
	/* Clear old selection */
	if (menu->selection.item) {
		wlr_scene_node_set_enabled(
			&menu->selection.item->normal.tree->node, true);
		wlr_scene_node_set_enabled(
			&menu->selection.item->selected.tree->node, false);
	}
	/* Set new selection */
	if (item) {
		wlr_scene_node_set_enabled(&item->normal.tree->node, false);
		wlr_scene_node_set_enabled(&item->selected.tree->node, true);
	}
	menu->selection.item = item;
}

static void
close_all_submenus(struct menu *menu)
{
	struct menuitem *item;
	wl_list_for_each(item, &menu->menuitems, link) {
		if (item->submenu) {
			wlr_scene_node_set_enabled(
				&item->submenu->scene_tree->node, false);
			close_all_submenus(item->submenu);
		}
	}
	menu->selection.menu = NULL;
}

static void
menu_close(struct menu *menu)
{
	if (!menu) {
		wlr_log(WLR_ERROR, "Trying to close non exiting menu");
		return;
	}
	wlr_scene_node_set_enabled(&menu->scene_tree->node, false);
	menu_set_selection(menu, NULL);
	if (menu->selection.menu) {
		menu_close(menu->selection.menu);
		menu->selection.menu = NULL;
	}
}

static void
menu_process_item_selection(struct menuitem *item)
{
	assert(item);

	if (!item->selectable) {
		return;
	}

	/* We are on an item that has new mouse-focus */
	menu_set_selection(item->parent, item);
	if (item->parent->selection.menu) {
		/* Close old submenu tree */
		menu_close(item->parent->selection.menu);
	}

	if (item->submenu) {
		/* Sync the triggering view */
		item->submenu->triggered_by_view = item->parent->triggered_by_view;
		/* Ensure the submenu has its parent set correctly */
		item->submenu->parent = item->parent;
		/* And open the new submenu tree */
		wlr_scene_node_set_enabled(
			&item->submenu->scene_tree->node, true);
	}
	item->parent->selection.menu = item->submenu;
}

/* Get the deepest submenu with active item selection or the root menu itself */
static struct menu *
get_selection_leaf(struct server *server)
{
	struct menu *menu = server->menu_current;
	if (!menu) {
		return NULL;
	}

	while (menu->selection.menu) {
		if (!menu->selection.menu->selection.item) {
			return menu;
		}
		menu = menu->selection.menu;
	}

	return menu;
}

/* Selects the next or previous sibling of the currently selected item */
static void
menu_item_select(struct server *server, bool forward)
{
	struct menu *menu = get_selection_leaf(server);
	if (!menu) {
		return;
	}

	struct menuitem *item = NULL;
	struct menuitem *selection = menu->selection.item;
	struct wl_list *start = selection ? &selection->link : &menu->menuitems;
	struct wl_list *current = start;
	while (!item || !item->selectable) {
		current = forward ? current->next : current->prev;
		if (current == start) {
			return;
		}
		if (current == &menu->menuitems) {
			/* Allow wrap around */
			item = NULL;
			continue;
		}
		item = wl_container_of(current, item, link);
	}

	menu_process_item_selection(item);
}

static bool
menu_execute_item(struct menuitem *item)
{
	assert(item);

	if (item->submenu || !item->selectable) {
		/* We received a click on a separator or item that just opens a submenu */
		return false;
	}

	actions_run(item->parent->triggered_by_view,
		item->parent->server, &item->actions, 0);

	/**
	 * We close the menu here to provide a faster feedback to the user.
	 * We do that without resetting the input state so src/cursor.c
	 * can do its own clean up on the following RELEASE event.
	 */
	menu_close(item->parent->server->menu_current);
	item->parent->server->menu_current = NULL;

	return true;
}

/* Public API */
void
menu_open(struct menu *menu, int x, int y)
{
	assert(menu);
	if (menu->server->menu_current) {
		menu_close(menu->server->menu_current);
	}
	close_all_submenus(menu);
	menu_set_selection(menu, NULL);
	menu_configure(menu, x, y, LAB_MENU_OPEN_AUTO);
	wlr_scene_node_set_enabled(&menu->scene_tree->node, true);
	menu->server->menu_current = menu;
	menu->server->input_mode = LAB_INPUT_STATE_MENU;
}

void
menu_close_root(struct server *server)
{
	assert(server->input_mode == LAB_INPUT_STATE_MENU);
	if (server->menu_current) {
		menu_close(server->menu_current);
		server->menu_current = NULL;
	}
	server->input_mode = LAB_INPUT_STATE_PASSTHROUGH;
}

/* Keyboard based selection */
void
menu_item_select_next(struct server *server)
{
	menu_item_select(server, /* forward */ true);
}

void
menu_item_select_previous(struct server *server)
{
	menu_item_select(server, /* forward */ false);
}

bool
menu_call_selected_actions(struct server *server)
{
	struct menu *menu = get_selection_leaf(server);
	if (!menu || !menu->selection.item) {
		return false;
	}

	return menu_execute_item(menu->selection.item);
}

void
menu_submenu_enter(struct server *server)
{
	/* Selects the first item on the submenu attached to the current selection */
	struct menu *menu = get_selection_leaf(server);
	if (!menu || !menu->selection.menu) {
		return;
	}

	struct wl_list *start = &menu->selection.menu->menuitems;
	struct wl_list *current = start;
	struct menuitem *item = NULL;
	while (!item || !item->selectable) {
		current = current->next;
		if (current == start) {
			return;
		}
		item = wl_container_of(current, item, link);
	}

	menu_process_item_selection(item);
}

void
menu_submenu_leave(struct server *server)
{
	/* Re-selects the selected item on the parent menu of the current selection */
	struct menu *menu = get_selection_leaf(server);
	if (!menu || !menu->parent || !menu->parent->selection.item) {
		return;
	}

	menu_process_item_selection(menu->parent->selection.item);
}

/* Mouse based selection */
void
menu_process_cursor_motion(struct wlr_scene_node *node)
{
	assert(node && node->data);
	struct menuitem *item = node_menuitem_from_node(node);

	if (item->selectable && node == &item->selected.tree->node) {
		/* We are on an already selected item */
		return;
	}

	menu_process_item_selection(item);
}

bool
menu_call_actions(struct wlr_scene_node *node)
{
	assert(node && node->data);
	struct menuitem *item = node_menuitem_from_node(node);

	return menu_execute_item(item);
}
