#include <assert.h>
#include "action.h"
#include "common/scaled_font_buffer.h"
#include "common/macros.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "node.h"
#include "prompt.h"
#include "theme.h"
#include <wlr/types/wlr_scene.h>


enum prompt_btn_type {
	PROMPT_BTN_NO = 0,
	PROMPT_BTN_YES = 1,
};

struct prompt_button {
	enum prompt_btn_type type;
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *bg;
	struct scaled_font_buffer *text;
};

static struct {
	struct prompt prompt;
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *bg;
	struct scaled_font_buffer *text;
	struct prompt_button btn_yes;
	struct prompt_button btn_no;
} ctx;

#define BTN_HEIGHT 40
#define BTN_WIDTH 100
#define BTN_MARGIN 20
#define PROMPT_WIDTH (BTN_WIDTH * 2 + BTN_MARGIN * 3)

float bg_color_yes[4] = { 0.1, 0.5, 0.1, 1.0 };
float bg_color_no[4] = { 0.5, 0.1, 0.1, 1.0 };

static void
center(struct wlr_scene_node *node,
		int outer_width, int outer_height,
		int inner_width, int inner_height)
{
	wlr_scene_node_set_position(node,
		(outer_width - inner_width) / 2,
		(outer_height - inner_height) / 2);
}

static void
prompt_reposition(void)
{
	int x, y;
	int text_height = MIN(font_height(&rc.font_osd), BTN_HEIGHT);
	int height = text_height + BTN_HEIGHT + BTN_MARGIN * 3;
	wlr_scene_rect_set_size(ctx.bg, PROMPT_WIDTH, height);

	/* Position the whole prompt */
	struct output *output = output_nearest_to_cursor(ctx.prompt.server);
	if (!output) {
		wlr_log(WLR_ERROR, "no output to position prompt against");
		wlr_scene_node_set_position(&ctx.tree->node, 300, 100);
	} else {
		center(&ctx.tree->node,
			output->usable_area.width, output->usable_area.height,
			PROMPT_WIDTH, height);
	}

	/* Main text */
	int prompt_text_width = font_width(&rc.font_osd, ctx.prompt.label_prompt);
	x = BTN_MARGIN + (PROMPT_WIDTH - BTN_MARGIN * 2 - prompt_text_width) / 2;
	wlr_scene_node_set_position(&ctx.text->scene_buffer->node, x, BTN_MARGIN);

	/* Center text within the buttons */
	center(&ctx.btn_yes.text->scene_buffer->node, BTN_WIDTH, BTN_HEIGHT,
		font_width(&rc.font_osd, ctx.prompt.label_yes), text_height);
	center(&ctx.btn_no.text->scene_buffer->node, BTN_WIDTH, BTN_HEIGHT,
		font_width(&rc.font_osd, ctx.prompt.label_no), text_height);

	/* And finally position the buttons within the prompt */
	x = BTN_MARGIN + (PROMPT_WIDTH - BTN_WIDTH * 2 - BTN_MARGIN * 3) / 2;
	y = text_height + BTN_MARGIN * 2;
	wlr_scene_node_set_position(&ctx.btn_yes.tree->node, x, y);
	x += BTN_WIDTH + BTN_MARGIN;
	wlr_scene_node_set_position(&ctx.btn_no.tree->node, x, y);
}

static void
prompt_create(void)
{
	ctx.tree = wlr_scene_tree_create(&ctx.prompt.server->scene->tree);
	ctx.bg = wlr_scene_rect_create(ctx.tree, 0, 0, rc.theme->osd_bg_color);
	ctx.text = scaled_font_buffer_create(ctx.tree);

	ctx.btn_yes.type = PROMPT_BTN_YES;
	ctx.btn_yes.tree = wlr_scene_tree_create(ctx.tree);
	ctx.btn_yes.bg = wlr_scene_rect_create(ctx.btn_yes.tree, BTN_WIDTH, BTN_HEIGHT, bg_color_yes);
	ctx.btn_yes.text = scaled_font_buffer_create(ctx.btn_yes.tree);

	ctx.btn_no.type = PROMPT_BTN_NO;
	ctx.btn_no.tree = wlr_scene_tree_create(ctx.tree);
	ctx.btn_no.bg = wlr_scene_rect_create(ctx.btn_no.tree, BTN_WIDTH, BTN_HEIGHT, bg_color_no);
	ctx.btn_no.text = scaled_font_buffer_create(ctx.btn_no.tree);

	node_descriptor_create(&ctx.btn_yes.tree->node, LAB_NODE_DESC_PROMPT_BUTTON, &ctx.btn_yes);
	node_descriptor_create(&ctx.btn_no.tree->node, LAB_NODE_DESC_PROMPT_BUTTON, &ctx.btn_no);
}

static void
prompt_update(void)
{
	if (!ctx.tree) {
		prompt_create();
	}

	scaled_font_buffer_update(ctx.text, ctx.prompt.label_prompt,
		PROMPT_WIDTH - BTN_MARGIN * 2, &rc.font_osd,
		rc.theme->osd_label_text_color,
		rc.theme->osd_bg_color, /* arrow */ NULL);

	scaled_font_buffer_update(ctx.btn_yes.text, ctx.prompt.label_yes,
		BTN_WIDTH, &rc.font_osd, rc.theme->osd_label_text_color,
		bg_color_yes, /* arrow */ NULL);

	scaled_font_buffer_update(ctx.btn_no.text, ctx.prompt.label_no,
		BTN_WIDTH, &rc.font_osd, rc.theme->osd_label_text_color,
		bg_color_no, /* arrow */ NULL);

	prompt_reposition();
}

void
prompt_show(struct prompt prompt)
{
	assert(prompt.label_prompt);
	assert(prompt.label_yes);
	assert(prompt.label_no);

	ctx.prompt = prompt;
	prompt_update();
	wlr_scene_node_set_enabled(&ctx.tree->node, true);
}

void
prompt_handle_button(struct wlr_scene_node *node)
{
	wlr_scene_node_set_enabled(&ctx.tree->node, false);

	struct prompt_button *btn = node_prompt_button_from_node(node);
	struct wl_list *branch = btn->type == PROMPT_BTN_YES
		? ctx.prompt.branch_then
		: ctx.prompt.branch_else;

	actions_run(ctx.prompt.view, ctx.prompt.server, branch, /*resize_edges*/ 0);
}
