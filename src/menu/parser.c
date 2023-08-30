// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include "action.h"
#include "common/buf.h"
#include "common/font.h"
#include "common/list.h"
#include "common/match.h"
#include "common/mem.h"
#include "common/nodename.h"
#include "common/scaled_font_buffer.h"
#include "common/scene-helpers.h"
#include "common/string-helpers.h"
#include "labwc.h"
#include "menu.h"
#include "node.h"
#include "theme.h"

/* state-machine variables for processing <item></item> */
static bool in_item;
static struct menuitem *current_item;
static struct action *current_item_action;

static int menu_level;
static struct menu *current_menu;

/* vector for <menu id="" label=""> elements */
static struct menu *menus;
static int nr_menus, alloc_menus;

static struct menu *
menu_create(struct server *server, const char *id, const char *label)
{
	if (nr_menus == alloc_menus) {
		alloc_menus = (alloc_menus + 16) * 2;
		menus = xrealloc(menus, alloc_menus * sizeof(struct menu));
	}
	struct menu *menu = menus + nr_menus;
	memset(menu, 0, sizeof(*menu));
	nr_menus++;
	wl_list_init(&menu->menuitems);
	menu->id = xstrdup(id);
	menu->label = xstrdup(label ? label : id);
	menu->parent = current_menu;
	menu->server = server;
	menu->size.width = server->theme->menu_min_width;
	/* menu->size.height will be kept up to date by adding items */
	menu->scene_tree = wlr_scene_tree_create(server->menu_tree);
	wlr_scene_node_set_enabled(&menu->scene_tree->node, false);
	return menu;
}

struct menu *
menu_get_by_id(const char *id)
{
	if (!id) {
		return NULL;
	}
	struct menu *menu;
	for (int i = 0; i < nr_menus; ++i) {
		menu = menus + i;
		if (!strcmp(menu->id, id)) {
			return menu;
		}
	}
	return NULL;
}

static void
menu_update_width(struct menu *menu)
{
	struct menuitem *item;
	struct theme *theme = menu->server->theme;
	int max_width = theme->menu_min_width;

	/* Get widest menu item, clamped by menu_max_width */
	wl_list_for_each(item, &menu->menuitems, link) {
		if (item->native_width > max_width) {
			max_width = item->native_width < theme->menu_max_width
				? item->native_width : theme->menu_max_width;
		}
	}
	menu->size.width = max_width + 2 * theme->menu_item_padding_x;

	/* Update all items for the new size */
	wl_list_for_each(item, &menu->menuitems, link) {
		wlr_scene_rect_set_size(
			lab_wlr_scene_get_rect(item->normal.background),
			menu->size.width, item->height);

		if (!item->selected.background) {
			/* This is a separator. They don't have a selected background. */
			wlr_scene_rect_set_size(
				lab_wlr_scene_get_rect(item->normal.text),
				menu->size.width - 2 * theme->menu_separator_padding_width,
				theme->menu_separator_line_thickness);
		} else {
			/* Usual menu item */
			wlr_scene_rect_set_size(
				lab_wlr_scene_get_rect(item->selected.background),
				menu->size.width, item->height);
			if (item->native_width > max_width || item->submenu) {
				scaled_font_buffer_set_max_width(item->normal.buffer,
					max_width);
				scaled_font_buffer_set_max_width(item->selected.buffer,
					max_width);
			}
		}
	}
}

static void
post_processing(struct server *server)
{
	struct menu *menu;
	for (int i = 0; i < nr_menus; ++i) {
		menu = menus + i;
		menu_update_width(menu);
	}
}

static void
validate_menu(struct menu *menu)
{
	struct menuitem *item;
	struct action *action, *action_tmp;
	wl_list_for_each(item, &menu->menuitems, link) {
		wl_list_for_each_safe(action, action_tmp, &item->actions, link) {
			if (!action_is_valid(action)) {
				wl_list_remove(&action->link);
				action_free(action);
				wlr_log(WLR_ERROR, "Removed invalid menu action");
			}
		}
	}
}

static void
validate(struct server *server)
{
	for (int i = 0; i < nr_menus; ++i) {
		validate_menu(menus + i);
	}
}

static struct menuitem *
item_create(struct menu *menu, const char *text, bool show_arrow)
{
	struct menuitem *menuitem = znew(*menuitem);
	menuitem->parent = menu;
	menuitem->selectable = true;
	struct server *server = menu->server;
	struct theme *theme = server->theme;

	const char *arrow = show_arrow ? "›" : NULL;

	if (!menu->item_height) {
		menu->item_height = font_height(&rc.font_menuitem)
			+ 2 * theme->menu_item_padding_y;
	}
	menuitem->height = menu->item_height;

	int x, y;
	menuitem->native_width = font_width(&rc.font_menuitem, text);
	if (arrow) {
		menuitem->native_width += font_width(&rc.font_menuitem, arrow);
	}

	/* Menu item root node */
	menuitem->tree = wlr_scene_tree_create(menu->scene_tree);
	node_descriptor_create(&menuitem->tree->node,
		LAB_NODE_DESC_MENUITEM, menuitem);

	/* Tree for each state to hold background and text buffer */
	menuitem->normal.tree = wlr_scene_tree_create(menuitem->tree);
	menuitem->selected.tree = wlr_scene_tree_create(menuitem->tree);

	/* Item background nodes */
	menuitem->normal.background = &wlr_scene_rect_create(
		menuitem->normal.tree,
		menu->size.width, menu->item_height,
		theme->menu_items_bg_color)->node;
	menuitem->selected.background = &wlr_scene_rect_create(
		menuitem->selected.tree,
		menu->size.width, menu->item_height,
		theme->menu_items_active_bg_color)->node;

	/* Font nodes */
	menuitem->normal.buffer = scaled_font_buffer_create(menuitem->normal.tree);
	menuitem->selected.buffer = scaled_font_buffer_create(menuitem->selected.tree);
	if (!menuitem->normal.buffer || !menuitem->selected.buffer) {
		wlr_log(WLR_ERROR, "Failed to create menu item '%s'", text);
		/**
		 * Destroying the root node will destroy everything,
		 * including the node descriptor and scaled_font_buffers.
		 */
		wlr_scene_node_destroy(&menuitem->tree->node);
		free(menuitem);
		return NULL;
	}
	menuitem->normal.text = &menuitem->normal.buffer->scene_buffer->node;
	menuitem->selected.text = &menuitem->selected.buffer->scene_buffer->node;

	/* Font buffers */
	scaled_font_buffer_update(menuitem->normal.buffer, text, menuitem->native_width,
		&rc.font_menuitem, theme->menu_items_text_color, arrow);
	scaled_font_buffer_update(menuitem->selected.buffer, text, menuitem->native_width,
		&rc.font_menuitem, theme->menu_items_active_text_color, arrow);

	/* Center font nodes */
	x = theme->menu_item_padding_x;
	y = (menu->item_height - menuitem->normal.buffer->height) / 2;
	wlr_scene_node_set_position(menuitem->normal.text, x, y);
	y = (menu->item_height - menuitem->selected.buffer->height) / 2;
	wlr_scene_node_set_position(menuitem->selected.text, x, y);

	/* Position the item in relation to its menu */
	wlr_scene_node_set_position(&menuitem->tree->node, 0, menu->size.height);

	/* Hide selected state */
	wlr_scene_node_set_enabled(&menuitem->selected.tree->node, false);

	/* Update menu extents */
	menu->size.height += menuitem->height;

	wl_list_append(&menu->menuitems, &menuitem->link);
	wl_list_init(&menuitem->actions);
	return menuitem;
}

static struct menuitem *
separator_create(struct menu *menu, const char *label)
{
	struct menuitem *menuitem = znew(*menuitem);
	menuitem->parent = menu;
	menuitem->selectable = false;
	struct server *server = menu->server;
	struct theme *theme = server->theme;
	menuitem->height = theme->menu_separator_line_thickness +
			2 * theme->menu_separator_padding_height;

	menuitem->tree = wlr_scene_tree_create(menu->scene_tree);
	node_descriptor_create(&menuitem->tree->node,
		LAB_NODE_DESC_MENUITEM, menuitem);
	menuitem->normal.tree = wlr_scene_tree_create(menuitem->tree);
	menuitem->normal.background = &wlr_scene_rect_create(
		menuitem->normal.tree,
		menu->size.width, menuitem->height,
		theme->menu_items_bg_color)->node;

	int width = menu->size.width - 2 * theme->menu_separator_padding_width;
	menuitem->normal.text = &wlr_scene_rect_create(
		menuitem->normal.tree,
		width > 0 ? width : 0,
		theme->menu_separator_line_thickness,
		theme->menu_separator_color)->node;

	wlr_scene_node_set_position(&menuitem->tree->node, 0, menu->size.height);

	/* Vertically center-align separator line */
	wlr_scene_node_set_position(menuitem->normal.text,
		theme->menu_separator_padding_width,
		theme->menu_separator_padding_height);

	menu->size.height += menuitem->height;
	wl_list_append(&menu->menuitems, &menuitem->link);
	wl_list_init(&menuitem->actions);
	return menuitem;
}

/*
 * Handle the following:
 * <item label="">
 *   <action name="">
 *     <command></command>
 *   </action>
 * </item>
 */
static void
fill_item(char *nodename, char *content)
{
	string_truncate_at_pattern(nodename, ".item.menu");

	/* <item label=""> defines the start of a new item */
	if (!strcmp(nodename, "label")) {
		current_item = item_create(current_menu, content, false);
		current_item_action = NULL;
	} else if (!current_item) {
		wlr_log(WLR_ERROR, "expect <item label=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else if (!strcmp(nodename, "icon")) {
		/*
		 * Do nothing as we don't support menu icons - just avoid
		 * logging errors if a menu.xml file contains icon="" entries.
		 */
	} else if (!strcmp(nodename, "name.action")) {
		current_item_action = action_create(content);
		if (current_item_action) {
			wl_list_append(&current_item->actions,
				&current_item_action->link);
		}
	} else if (!current_item_action) {
		wlr_log(WLR_ERROR, "expect <action name=\"\"> element first. "
			"nodename: '%s' content: '%s'", nodename, content);
	} else {
		action_arg_from_xml_node(current_item_action, nodename, content);
	}
}

static void
item_destroy(struct menuitem *item)
{
	wl_list_remove(&item->link);
	action_list_free(&item->actions);
	wlr_scene_node_destroy(&item->tree->node);
	free(item);
}

/*
 * We support XML CDATA for <command> in menu.xml in order to provide backward
 * compatibility with obmenu-generator. For example:
 *
 * <menu id="" label="">
 *   <item label="">
 *     <action name="Execute">
 *       <command><![CDATA[xdg-open .]]></command>
 *     </action>
 *   </item>
 * </menu>
 *
 * <execute> is an old, deprecated openbox variety of <command>. We support it
 * for backward compatibility with old openbox-menu generators. It has the same
 * function and <command>
 *
 * The match_glob() wildcard allows for nested menus giving nodenames with
 * ...menu.menu... or ...menu.menu.menu... and so on.
 */
static bool
nodename_supports_cdata(char *nodename)
{
	return match_glob("command.action.item.*menu.openbox_menu", nodename)
		|| match_glob("execute.action.item.*menu.openbox_menu", nodename);
}

static void
entry(xmlNode *node, char *nodename, char *content)
{
	if (!nodename) {
		return;
	}
	xmlChar *cdata = NULL;
	if (!content && nodename_supports_cdata(nodename)) {
		cdata = xmlNodeGetContent(node);
	}
	if (!content && !cdata) {
		return;
	}
	string_truncate_at_pattern(nodename, ".openbox_menu");
	if (getenv("LABWC_DEBUG_MENU_NODENAMES")) {
		printf("%s: %s\n", nodename, content ? content : (char *)cdata);
	}
	if (in_item) {
		fill_item(nodename, content ? content : (char *)cdata);
	}
	xmlFree(cdata);
}

static void
process_node(xmlNode *node)
{
	static char buffer[256];

	char *content = (char *)node->content;
	if (xmlIsBlankNode(node)) {
		return;
	}
	char *name = nodename(node, buffer, sizeof(buffer));
	entry(node, name, content);
}

static void xml_tree_walk(xmlNode *node, struct server *server);

static void
traverse(xmlNode *n, struct server *server)
{
	xmlAttr *attr;

	process_node(n);
	for (attr = n->properties; attr; attr = attr->next) {
		xml_tree_walk(attr->children, server);
	}
	xml_tree_walk(n->children, server);
}

static int
nr_parents(xmlNode *n)
{
	assert(n);
	int i = 0;
	for (xmlNode *node = n->parent; node && i < INT_MAX; ++i) {
		node = node->parent;
	}
	return i;
}

/*
 * <menu> elements have three different roles:
 *  * Definition of (sub)menu - has ID, LABEL and CONTENT
 *  * Menuitem of pipemenu type - has EXECUTE and LABEL
 *  * Menuitem of submenu type - has ID only
 */
static void
handle_menu_element(xmlNode *n, struct server *server)
{
	char *label = (char *)xmlGetProp(n, (const xmlChar *)"label");
	char *execute = (char *)xmlGetProp(n, (const xmlChar *)"execute");
	char *id = (char *)xmlGetProp(n, (const xmlChar *)"id");

	if (execute) {
		wlr_log(WLR_ERROR, "we do not support pipemenus");
	} else if ((label && id) || (id && nr_parents(n) == 2)) {
		/*
		 * (label && id) refers to <menu id="" label=""> which is an
		 * inline menu definition.
		 *
		 * (id && nr_parents(n) == 2) refers to:
		 * <openbox_menu>
		 *   <menu id="">
		 *   </menu>
		 * </openbox>
		 *
		 * which is the highest level a menu can be defined at.
		 *
		 * Openbox spec requires a label="" defined here, but it is
		 * actually pointless so we handle it with or without the label
		 * attritute to make it easier for users to define "root-menu"
		 * and "client-menu".
		 */
		struct menu **submenu = NULL;
		if (menu_level > 0) {
			/*
			 * In a nested (inline) menu definition we need to
			 * create an item pointing to the new submenu
			 */
			current_item = item_create(current_menu, label, true);
			if (current_item) {
				submenu = &current_item->submenu;
			} else {
				submenu = NULL;
			}
		}
		++menu_level;
		current_menu = menu_create(server, id, label);
		if (submenu) {
			*submenu = current_menu;
		}
		traverse(n, server);
		current_menu = current_menu->parent;
		--menu_level;
	} else if (id) {
		/*
		 * <menu id=""> creates an entry which points to a menu
		 * defined elsewhere
		 */
		struct menu *menu = menu_get_by_id(id);
		if (menu) {
			current_item = item_create(current_menu, menu->label, true);
			if (current_item) {
				current_item->submenu = menu;
			}
		} else {
			wlr_log(WLR_ERROR, "no menu with id '%s'", id);
		}
	}
	zfree(label);
	zfree(execute);
	zfree(id);
}

/* This can be one of <separator> and <separator label=""> */
static void
handle_separator_element(xmlNode *n)
{
	char *label = (char *)xmlGetProp(n, (const xmlChar *)"label");
	current_item = separator_create(current_menu, label);
	if (label) {
		free(label);
	}
}

static void
xml_tree_walk(xmlNode *node, struct server *server)
{
	for (xmlNode *n = node; n && n->name; n = n->next) {
		if (!strcasecmp((char *)n->name, "comment")) {
			continue;
		}
		if (!strcasecmp((char *)n->name, "menu")) {
			handle_menu_element(n, server);
			continue;
		}
		if (!strcasecmp((char *)n->name, "separator")) {
			handle_separator_element(n);
			continue;
		}
		if (!strcasecmp((char *)n->name, "item")) {
			in_item = true;
			traverse(n, server);
			in_item = false;
			continue;
		}
		traverse(n, server);
	}
}

static void
parse_xml(const char *filename, struct server *server)
{
	FILE *stream;
	char *line = NULL;
	size_t len = 0;
	struct buf b;
	static char menuxml[4096] = { 0 };

	if (!rc.config_dir) {
		return;
	}
	snprintf(menuxml, sizeof(menuxml), "%s/%s", rc.config_dir, filename);

	stream = fopen(menuxml, "r");
	if (!stream) {
		wlr_log(WLR_ERROR, "cannot read %s", menuxml);
		return;
	}
	wlr_log(WLR_INFO, "read menu file %s", menuxml);
	buf_init(&b);
	while (getline(&line, &len, stream) != -1) {
		char *p = strrchr(line, '\n');
		if (p) {
			*p = '\0';
		}
		buf_add(&b, line);
	}
	free(line);
	fclose(stream);
	xmlDoc *d = xmlParseMemory(b.buf, b.len);
	if (!d) {
		wlr_log(WLR_ERROR, "xmlParseMemory()");
		goto err;
	}
	xml_tree_walk(xmlDocGetRootElement(d), server);
	xmlFreeDoc(d);
	xmlCleanupParser();
err:
	free(b.buf);
}

static void
menu_hide_submenu(const char *id)
{
	struct menu *menu, *hide_menu;
	hide_menu = menu_get_by_id(id);
	if (!hide_menu) {
		return;
	}
	for (int i = 0; i < nr_menus; ++i) {
		menu = menus + i;
		bool should_reposition = false;
		struct menuitem *item, *next;
		wl_list_for_each_safe(item, next, &menu->menuitems, link) {
			if (item->submenu == hide_menu) {
				item_destroy(item);
				should_reposition = true;
			}
		}

		if (!should_reposition) {
			continue;
		}
		/* Re-position items vertically */
		menu->size.height = 0;
		wl_list_for_each(item, &menu->menuitems, link) {
			wlr_scene_node_set_position(&item->tree->node, 0,
				menu->size.height);
			menu->size.height += item->height;
		}
	}
}

static void
init_rootmenu(struct server *server)
{
	struct menu *menu = menu_get_by_id("root-menu");

	/* Default menu if no menu.xml found */
	if (!menu) {
		current_menu = NULL;
		menu = menu_create(server, "root-menu", "");
	}
	if (wl_list_empty(&menu->menuitems)) {
		current_item = item_create(menu, _("Reconfigure"), false);
		fill_item("name.action", "Reconfigure");
		current_item = item_create(menu, _("Exit"), false);
		fill_item("name.action", "Exit");
	}
}

static void
init_windowmenu(struct server *server)
{
	struct menu *menu = menu_get_by_id("client-menu");

	/* Default menu if no menu.xml found */
	if (!menu) {
		current_menu = NULL;
		menu = menu_create(server, "client-menu", "");
	}
	if (wl_list_empty(&menu->menuitems)) {
		current_item = item_create(menu, _("Minimize"), false);
		fill_item("name.action", "Iconify");
		current_item = item_create(menu, _("Maximize"), false);
		fill_item("name.action", "ToggleMaximize");
		current_item = item_create(menu, _("Fullscreen"), false);
		fill_item("name.action", "ToggleFullscreen");
		current_item = item_create(menu, _("Decorations"), false);
		fill_item("name.action", "ToggleDecorations");
		current_item = item_create(menu, _("AlwaysOnTop"), false);
		fill_item("name.action", "ToggleAlwaysOnTop");

		/* Workspace sub-menu */
		struct menu *workspace_menu = menu_create(server, "workspaces", "");
		current_item = item_create(workspace_menu, _("Move left"), false);
		/*
		 * <action name="SendToDesktop"><follow> is true by default so
		 * GoToDesktop will be called as part of the action.
		 */
		fill_item("name.action", "SendToDesktop");
		fill_item("to.action", "left");
		current_item = item_create(workspace_menu, _("Move right"), false);
		fill_item("name.action", "SendToDesktop");
		fill_item("to.action", "right");
		current_item = item_create(menu, _("Workspace"), true);
		current_item->submenu = workspace_menu;

		current_item = item_create(menu, _("Close"), false);
		fill_item("name.action", "Close");
	}

	if (wl_list_length(&rc.workspace_config.workspaces) == 1) {
		menu_hide_submenu("workspaces");
	}
}

void
menu_init(struct server *server)
{
	parse_xml("menu.xml", server);
	init_rootmenu(server);
	init_windowmenu(server);
	post_processing(server);
	validate(server);
}

void
menu_finish(void)
{
	struct menu *menu;
	for (int i = 0; i < nr_menus; ++i) {
		menu = menus + i;
		struct menuitem *item, *next;
		wl_list_for_each_safe(item, next, &menu->menuitems, link) {
			item_destroy(item);
		}
		/**
		 * Destroying the root node will destroy everything,
		 * including node descriptors and scaled_font_buffers.
		 */
		wlr_scene_node_destroy(&menu->scene_tree->node);
	}
	zfree(menus);
	alloc_menus = 0;
	nr_menus = 0;
}


void
menu_reconfigure(struct server *server)
{
	menu_finish();
	server->menu_current = NULL;
	menu_init(server);
}
