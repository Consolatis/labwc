#include <assert.h>
#include <wlr/util/log.h>
#include "input/cursor.h"
#include "node.h"
#include "ui.h"

/* Internal helpers */

static bool
_el_is_some_parent_of(struct ui_element *parent, struct ui_element *el)
{
	if (!parent || !el) {
		return false;
	}
	for (; el && el != parent; el = el->parent);
	return el != NULL;
}

static struct ui_element *
_get_shared_root(struct ui_element *el1, struct ui_element *el2)
{
	struct ui_element *tmp;
	for (; el1; el1 = el1->parent) {
		for (tmp = el2; tmp; tmp = tmp->parent) {
			if (tmp == el1 || _el_is_some_parent_of(el1, tmp)) {
				return el1;
			}
		}
	}
	return NULL;
}

static void
_handle_mouse_out(struct ui_element *new_el, struct ui_element *old_el,
		struct ui_element *shared_root, struct cursor_context *ctx)
{
	if (!old_el || _el_is_some_parent_of(old_el, new_el)) {
		/* Still in the hierachy of the old element */
		return;
	}

	/* Bubble up mouse_out events up to (but not including) the shared ancestor */
	struct ui_element *el;
	struct ui_context ui_ctx = {
		.action = UI_MOUSE_OUT,
		.ctx = ctx
	};
	for (el = old_el; el && el != shared_root; el = el->parent) {
		ui_bubble_up(el, &ui_ctx);
	}
}

static void
_handle_mouse_in(struct ui_element *new_el, struct ui_element *shared_root,
		struct cursor_context *ctx)
{
	struct ui_element *el;
	struct ui_context ui_ctx = {
		.action = UI_MOUSE_IN,
		.ctx = ctx
	};

	/* Bubble up mouse_in events up to the shared ancestor */
	for (el = new_el; el && el != shared_root; el = el->parent) {
		ui_bubble_up(el, &ui_ctx);
	}
}

static void
_notify_move(struct ui_element *el, struct cursor_context *ctx)
{
	struct ui_context ui_ctx = {
		.action = UI_MOUSE_MOVE,
		.ctx = ctx
	};
	ui_bubble_up(el, &ui_ctx);
}

/* Public API */
void
ui_handle_motion(struct cursor_context *ctx)
{
	static struct ui_element *prev_el = NULL;

	if (ctx->type == LAB_SSD_UI_ELEMENT) {
		struct ui_element *el = node_ui_element_from_node(ctx->node);
		if (prev_el == el) {
			/* still on the same element */
			_notify_move(el, ctx);
			return;
		}

		struct ui_element *shared_root = _get_shared_root(el, prev_el);
		_handle_mouse_out(el, prev_el, shared_root, ctx);
		_handle_mouse_in(el, shared_root, ctx);
		_notify_move(el, ctx);
		prev_el = el;
	} else if (prev_el) {
		_handle_mouse_out(NULL, prev_el, NULL, ctx);
		prev_el = NULL;
	}
}

void
ui_bubble_up(struct ui_element *self, struct ui_context *ctx)
{
	assert(ctx->action < UI_MAX_ACTION_LEN);

	for (; self != NULL; self = self->parent) {
		if (self->impl[ctx->action]) {
			ctx->propagate = false;
			self->impl[ctx->action](self, ctx);

			if (ctx->action == UI_MOUSE_IN || ctx->action == UI_MOUSE_OUT
					|| !ctx->propagate) {
				/* Mouse in / Mouse out bubbling is hardcoded */
				return;
			}
		}
	}
}

