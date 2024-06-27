/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_UI_H
#define LABWC_UI_H

#include <stdbool.h>
#include "config/mousebind.h"

enum ui_action {
	UI_MOUSE_IN = 0,
	UI_MOUSE_OUT,
	UI_MOUSE_MOVE,
	UI_MOUSE_CLICK,
	UI_MOUSE_AXIS,
	UI_MAX_ACTION_LEN
};

struct ui_element;
struct cursor_context;

struct ui_context {
	enum ui_action action;
	enum direction direction;    /* config/mousebind.h */
	struct cursor_context *ctx;

	/* can be set by handlers to keep the event bubbling up */
	bool propagate;
};

typedef void (*ui_impl)(struct ui_element *self, struct ui_context *ctx);

struct ui_element {
	struct ui_element *parent; /* May be NULL */
	void *data;                /* user pointer */

	ui_impl impl[UI_MAX_ACTION_LEN];
};

void ui_bubble_up(struct ui_element *self, struct ui_context *ctx);
void ui_handle_motion(struct cursor_context *ctx);

#endif /* LABWC_UI_H */
