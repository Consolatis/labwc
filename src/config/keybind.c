// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>
#include "common/list.h"
#include "common/mem.h"
#include "config/keybind.h"
#include "config/rcxml.h"
#include "labwc.h"

uint32_t
parse_modifier(const char *symname)
{
	if (!strcmp(symname, "S")) {
		return WLR_MODIFIER_SHIFT;
	} else if (!strcmp(symname, "C")) {
		return WLR_MODIFIER_CTRL;
	} else if (!strcmp(symname, "A")) {
		return WLR_MODIFIER_ALT;
	} else if (!strcmp(symname, "W")) {
		return WLR_MODIFIER_LOGO;
	} else {
		return 0;
	}
}

bool
keybind_the_same(struct keybind *a, struct keybind *b)
{
	assert(a && b);
	if (a->modifiers != b->modifiers || a->keysyms_len != b->keysyms_len) {
		return false;
	}
	for (size_t i = 0; i < a->keysyms_len; i++) {
		if (a->keysyms[i] != b->keysyms[i]) {
			return false;
		}
	}
	return true;
}

static void
update_keycodes_iter(struct xkb_keymap *keymap, xkb_keycode_t key, void *data)
{
	struct keybind *keybind;
	const xkb_keysym_t *syms;
	xkb_layout_index_t layout = *(int *)data;
	int nr_syms = xkb_keymap_key_get_syms_by_level(keymap, key, layout, 0, &syms);
	if (!nr_syms) {
		return;
	}
	for (int i = 0; i < nr_syms; i++) {
		xkb_keysym_t sym = syms[i];
		wl_list_for_each(keybind, &rc.keybinds, link) {
			for (size_t j = 0; j < keybind->keysyms_len; j++) {
				if (sym != keybind->keysyms[j]) {
					continue;
				}
				/* Found keycode for sym */
				if (keybind->keycodes_len == MAX_KEYCODES) {
					wlr_log(WLR_ERROR,
						"Already stored %lu keycodes for keybind",
						keybind->keycodes_len);
					break;
				}
				bool keycode_exists = false;
				for (size_t k = 0; k < keybind->keycodes_len; k++) {
					if (keybind->keycodes[k] == key) {
						keycode_exists = true;
						break;
					}
				}
				if (keycode_exists) {
					continue;
				}
				wlr_log(WLR_INFO, "Adding keycode 0x%x\t for sym 0x%x", key, sym);
				keybind->keycodes[keybind->keycodes_len++] = key;
			}
		}
	}
}

void
keybind_update_keycodes(struct server *server)
{
	struct xkb_state *state = server->seat.keyboard_group->keyboard.xkb_state;
	struct xkb_keymap *keymap = xkb_state_get_keymap(state);

	struct keybind *keybind;
	wl_list_for_each(keybind, &rc.keybinds, link) {
		keybind->keycodes_len = 0;
	}
	xkb_layout_index_t layouts = xkb_keymap_num_layouts(keymap);
	for (xkb_layout_index_t i = 0; i < layouts; i++) {
		wlr_log(WLR_ERROR, "Found layout %s", xkb_keymap_layout_get_name(keymap, i));
		xkb_keymap_key_for_each(keymap, update_keycodes_iter, &i);
	}
}

struct keybind *
keybind_create(const char *keybind)
{
	struct keybind *k = znew(*k);
	xkb_keysym_t keysyms[MAX_KEYSYMS];
	gchar **symnames = g_strsplit(keybind, "-", -1);
	for (size_t i = 0; symnames[i]; i++) {
		char *symname = symnames[i];
		uint32_t modifier = parse_modifier(symname);
		if (modifier != 0) {
			k->modifiers |= modifier;
		} else {
			xkb_keysym_t sym = xkb_keysym_to_lower(
				xkb_keysym_from_name(symname,
					XKB_KEYSYM_CASE_INSENSITIVE));
			if (sym == XKB_KEY_NoSymbol) {
				wlr_log(WLR_ERROR, "unknown keybind (%s)", symname);
				free(k);
				k = NULL;
				break;
			}
			keysyms[k->keysyms_len] = sym;
			k->keysyms_len++;
			if (k->keysyms_len == MAX_KEYSYMS) {
				wlr_log(WLR_ERROR, "There are a lot of fingers involved. "
					"We stopped counting at %u.", MAX_KEYSYMS);
				wlr_log(WLR_ERROR, "Offending keybind was %s", keybind);
				break;
			}
		}
	}
	g_strfreev(symnames);
	if (!k) {
		return NULL;
	}
	wl_list_append(&rc.keybinds, &k->link);
	k->keysyms = xmalloc(k->keysyms_len * sizeof(xkb_keysym_t));
	memcpy(k->keysyms, keysyms, k->keysyms_len * sizeof(xkb_keysym_t));
	wl_list_init(&k->actions);
	return k;
}
