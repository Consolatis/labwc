#include "action.h"
#include "common/scaled_font_buffer.h"
#include "common/macros.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "prompt.h"
#include "theme.h"
#include <wlr/types/wlr_scene.h>

static struct {
	struct prompt prompt;
	struct wlr_scene_tree *tree;
	struct wlr_scene_rect *bg;
	struct wlr_scene_rect *bg_yes;
	struct wlr_scene_rect *bg_no;
	struct scaled_font_buffer *text;
	struct scaled_font_buffer *text_yes;
	struct scaled_font_buffer *text_no;
} ctx;

#define PROMPT_WIDTH 640
//#define PROMPT_HEIGHT 480
#define BTN_HEIGHT 40
#define BTN_WIDTH 80
#define BTN_MARGIN 20

float fg[4] = { 0.0, 0.0, 0.0, 1.0 };
float bg_color[4] = { 0.1, 0.1, 0.5, 1.0 };
float bg_color_yes[4] = { 0.1, 0.5, 0.1, 1.0 };
float bg_color_no[4] = { 0.5, 0.1, 0.1, 1.0 };

static void
prompt_reposition(void)
{
	// FIXME: output_by_cursor -> center
	wlr_scene_node_set_position(&ctx.tree->node, 300, 100);

	int text_height = MIN(font_height(&rc.font_osd), BTN_HEIGHT);
	int height = text_height + BTN_HEIGHT + BTN_MARGIN * 3;
	int prompt_text_width = font_width(&rc.font_osd, ctx.prompt.label_prompt);
	// FIXME: misses margin offset
	int x = (PROMPT_WIDTH - BTN_MARGIN * 2 - prompt_text_width) / 2;

	wlr_scene_rect_set_size(ctx.bg, PROMPT_WIDTH, height);
	wlr_scene_node_set_position(&ctx.text->scene_buffer->node, x, BTN_MARGIN);

	x = (PROMPT_WIDTH - BTN_WIDTH * 2 - BTN_MARGIN * 3) / 2;
	int y = text_height + BTN_MARGIN * 2;
	wlr_scene_node_set_position(&ctx.text_yes->scene_buffer->node,
		x + (BTN_WIDTH - font_width(&rc.font_osd, ctx.prompt.label_yes)) / 2,
		y + (BTN_HEIGHT - text_height) / 2);
	wlr_scene_node_set_position(&ctx.bg_yes->node, x, y);
	x += BTN_WIDTH + BTN_MARGIN;
	wlr_scene_node_set_position(&ctx.text_no->scene_buffer->node,
		x + (BTN_WIDTH - font_width(&rc.font_osd, ctx.prompt.label_no)) / 2,
		y + (BTN_HEIGHT - text_height) / 2);
	wlr_scene_node_set_position(&ctx.bg_no->node, x, y);
}

static void
prompt_create(void)
{
	ctx.tree = wlr_scene_tree_create(&ctx.prompt.server->scene->tree);
	ctx.bg = wlr_scene_rect_create(ctx.tree, 0, 0, rc.theme->osd_bg_color);
	ctx.bg_yes = wlr_scene_rect_create(ctx.tree, BTN_WIDTH, BTN_HEIGHT, bg_color_yes);
	ctx.bg_no = wlr_scene_rect_create(ctx.tree, BTN_WIDTH, BTN_HEIGHT, bg_color_no);
	ctx.text = scaled_font_buffer_create(ctx.tree);
	ctx.text_yes = scaled_font_buffer_create(ctx.tree);
	ctx.text_no = scaled_font_buffer_create(ctx.tree);
}

static void
prompt_update(void)
{
	wlr_log(WLR_ERROR, "should update prompt with %s (and options %s and %s)",
		ctx.prompt.label_prompt, ctx.prompt.label_yes, ctx.prompt.label_no);

	if (!ctx.tree) {
		prompt_create();
	}

	scaled_font_buffer_update(ctx.text, ctx.prompt.label_prompt,
		PROMPT_WIDTH - BTN_MARGIN * 2, &rc.font_osd,
		rc.theme->osd_label_text_color,
		rc.theme->osd_bg_color, /* arrow */ NULL);

	scaled_font_buffer_update(ctx.text_yes, ctx.prompt.label_yes,
		BTN_WIDTH, &rc.font_osd, rc.theme->osd_label_text_color,
		bg_color_yes, /* arrow */ NULL);

	scaled_font_buffer_update(ctx.text_no, ctx.prompt.label_no,
		BTN_WIDTH, &rc.font_osd, rc.theme->osd_label_text_color,
		bg_color_no, /* arrow */ NULL);

	prompt_reposition();
}

void
prompt_show(struct prompt prompt)
{
	wlr_log(WLR_ERROR, "should render prompt with %s (and options %s and %s)",
		prompt.label_prompt, prompt.label_yes, prompt.label_no);
	ctx.prompt = prompt;
	prompt_update();
	wlr_scene_node_set_enabled(&ctx.tree->node, true);
	actions_run(prompt.view, prompt.server, prompt.branch_then, /*resize_edges*/ 0);
}
