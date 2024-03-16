// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/util/log.h>
#include "common/mem.h"
#include "config/rcxml.h"
#include "view.h"
#include "workspaces.h"
#include "labwc.h"
#include "osd.h"

/* includes '%', terminating 's' and NULL byte, 8 is enough for %-9999W */
#define LAB_FIELD_SINGLE_FMT_MAX_LEN 8
static_assert(LAB_FIELD_SINGLE_FMT_MAX_LEN <= 255, "fmt_len is a unsigned char");

/* forward declares */
typedef void field_set_fn_t(struct buf *buf, struct view *view, const char *format);
struct field_handlers {
	const char fmt_char;
	field_set_fn_t *fn;
};
static const struct field_handlers field_handlers[];

/* Internal helpers */

static const char *
get_app_id_or_class(struct view *view, bool trim)
{
	/*
	 * XWayland clients return WM_CLASS for 'app_id' so we don't need a
	 * special case for that here.
	 */
	const char *identifier = view_get_string_prop(view, "app_id");

	/* remove the first two nodes of 'org.' strings */
	if (trim && identifier && !strncmp(identifier, "org.", 4)) {
		char *p = (char *)identifier + 4;
		p = strchr(p, '.');
		if (p) {
			return ++p;
		}
	}
	return identifier;
}

static const char *
get_type(struct view *view, bool short_form)
{
	switch (view->type) {
	case LAB_XDG_SHELL_VIEW:
		return short_form ? "[W]" : "[xdg-shell]";
#if HAVE_XWAYLAND
	case LAB_XWAYLAND_VIEW:
		return short_form ? "[X]" : "[xwayland]";
#endif
	}
	return "???";
}

static const char *
get_title(struct view *view)
{
	return view_get_string_prop(view, "title");
}

static const char *
get_title_if_different(struct view *view)
{
	const char *identifier = get_app_id_or_class(view, /*trim*/ false);
	const char *title = get_title(view);
	if (!identifier) {
		return title;
	}
	return (!title || !strcmp(identifier, title)) ? NULL : title;
}

/* Field handlers */
static void
field_set_type(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: B (backend) */
	buf_add(buf, get_type(view, /*short_form*/ false));
}

static void
field_set_type_short(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: b (backend) */
	buf_add(buf, get_type(view, /*short_form*/ true));
}

static void
field_set_workspace(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: W */
	buf_add(buf, view->workspace->name);
}

static void
field_set_workspace_short(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: w */
	if (wl_list_length(&rc.workspace_config.workspaces) > 1) {
		buf_add(buf, view->workspace->name);
	}
}

static void
field_set_win_state(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: s */
	if (view->maximized) {
		buf_add(buf, "M");
	} else if (view->minimized) {
		buf_add(buf, "m");
	} else if (view->fullscreen) {
		buf_add(buf, "F");
	} else {
		buf_add(buf, " ");
	}
}

static void
field_set_win_state_all(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: S */
	buf_add(buf, view->minimized ? "m" : " ");
	buf_add(buf, view->maximized ? "M" : " ");
	buf_add(buf, view->fullscreen ? "F" : " ");
	/* TODO: add always-on-top and omnipresent ? */
}

static void
field_set_output(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: O */
	if (output_is_usable(view->output)) {
		buf_add(buf, view->output->wlr_output->name);
	}
}

static void
field_set_output_short(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: o */
	if (wl_list_length(&view->server->outputs) > 1 &&
			output_is_usable(view->output)) {
		buf_add(buf, view->output->wlr_output->name);
	}
}

static void
field_set_identifier(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: I */
	buf_add(buf, get_app_id_or_class(view, /*trim*/ false));
}

static void
field_set_identifier_trimmed(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: i */
	buf_add(buf, get_app_id_or_class(view, /*trim*/ true));
}

static void
field_set_title(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: T */
	buf_add(buf, get_title(view));
}

static void
field_set_title_short(struct buf *buf, struct view *view, const char *format)
{
	/* custom type identifier: t */
	buf_add(buf, get_title_if_different(view));
}

static void
field_set_custom(struct buf *buf, struct view *view, const char *format)
{

	if (!format) {
		wlr_log(WLR_ERROR, "Missing format for custom window switcher field");
		return;
	}

	char fmt[LAB_FIELD_SINGLE_FMT_MAX_LEN];
	unsigned char fmt_len = 0;

	struct buf field_result;
	buf_init(&field_result);

	char field_formatted[4096];

	for (const char *p = format; *p; p++) {
		if (!fmt_len) {
			if (*p == '%') {
				fmt[fmt_len++] = *p;
			} else {
				/*
				 * Just relay anything not part of a
				 * format string to the output buffer.
				 */
				buf_add_char(buf, *p);
			}
			continue;
		}

		/* Allow string formatting */
		/* TODO: add . for manual truncating? */
		if (*p == '-' || *p == '#' || (*p >= '0' && *p <= '9')) {
			if (fmt_len >= LAB_FIELD_SINGLE_FMT_MAX_LEN - 2) {
				/* Leave space for terminating 's' and NULL byte */
				wlr_log(WLR_ERROR,
					"single format string length exceeded: '%s'", p);
			} else {
				fmt[fmt_len++] = *p;
			}
			continue;
		}

		/* Handlers */
		for (unsigned char i = 0; i < LAB_FIELD_COUNT; i++) {
			if (*p != field_handlers[i].fmt_char) {
				continue;
			}

			/* Generate the actual content*/
			field_handlers[i].fn(&field_result, view, /*format*/ NULL);

			/* Throw it at snprintf to allow formatting / padding */
			fmt[fmt_len++] = 's';
			fmt[fmt_len++] = '\0';
			snprintf(field_formatted, sizeof(field_formatted),
				fmt, field_result.buf);

			/* And finally write it to the output buffer */
			buf_add(buf, field_formatted);
			goto reset_format;
		}

		wlr_log(WLR_ERROR,
			"invalid format character found for osd %s: '%c'",
			format, *p);

reset_format:
		/* Reset format string and tmp field result buffer */
		buf_clear(&field_result);
		fmt_len = 0;
	}
	free(field_result.buf);
}

static const struct field_handlers field_handlers[LAB_FIELD_COUNT] = {
	[LAB_FIELD_TYPE]               = { 'B', field_set_type },
	[LAB_FIELD_TYPE_SHORT]         = { 'b', field_set_type_short },
	[LAB_FIELD_WIN_STATE_ALL]      = { 'S', field_set_win_state_all },
	[LAB_FIELD_WIN_STATE]          = { 's', field_set_win_state },
	[LAB_FIELD_IDENTIFIER]         = { 'I', field_set_identifier },
	[LAB_FIELD_TRIMMED_IDENTIFIER] = { 'i', field_set_identifier_trimmed },
	[LAB_FIELD_WORKSPACE]          = { 'W', field_set_workspace },
	[LAB_FIELD_WORKSPACE_SHORT]    = { 'w', field_set_workspace_short },
	[LAB_FIELD_OUTPUT]             = { 'O', field_set_output },
	[LAB_FIELD_OUTPUT_SHORT]       = { 'o', field_set_output_short },
	[LAB_FIELD_TITLE]              = { 'T', field_set_title },
	[LAB_FIELD_TITLE_SHORT]        = { 't', field_set_title_short },
	/* fmt_char can never be matched so prevents LAB_FIELD_CUSTOM recursion */
	[LAB_FIELD_CUSTOM]             = { '\0', field_set_custom },
};

struct window_switcher_field *
osd_field_create(void)
{
	struct window_switcher_field *field = znew(*field);
	return field;

}

void
osd_field_arg_from_xml_node(struct window_switcher_field *field,
		const char *nodename, const char *content)
{
	if (!strcmp(nodename, "content")) {
		if (!strcmp(content, "type")) {
			field->content = LAB_FIELD_TYPE;
		} else if (!strcmp(content, "type_short")) {
			field->content = LAB_FIELD_TYPE_SHORT;
		} else if (!strcmp(content, "app_id")) {
			wlr_log(WLR_ERROR, "window-switcher field 'app_id' is deprecated");
			field->content = LAB_FIELD_IDENTIFIER;
		} else if (!strcmp(content, "identifier")) {
			field->content = LAB_FIELD_IDENTIFIER;
		} else if (!strcmp(content, "trimmed_identifier")) {
			field->content = LAB_FIELD_TRIMMED_IDENTIFIER;
		} else if (!strcmp(content, "title")) {
			/* Keep old defaults */
			field->content = LAB_FIELD_TITLE_SHORT;
		} else if (!strcmp(content, "workspace")) {
			field->content = LAB_FIELD_WORKSPACE;
		} else if (!strcmp(content, "state")) {
			field->content = LAB_FIELD_WIN_STATE;
		} else if (!strcmp(content, "output")) {
			/* Keep old defaults */
			field->content = LAB_FIELD_OUTPUT_SHORT;
		} else if (!strcmp(content, "custom")) {
			field->content = LAB_FIELD_CUSTOM;
		} else {
			wlr_log(WLR_ERROR, "bad windowSwitcher field '%s'", content);
		}
	} else if (!strcmp(nodename, "format")) {
		zfree(field->format);
		field->format = xstrdup(content);
	} else if (!strcmp(nodename, "width") && !strchr(content, '%')) {
		wlr_log(WLR_ERROR, "Invalid osd field width: %s, misses trailing %%", content);
	} else if (!strcmp(nodename, "width")) {
		field->width = atoi(content);
	} else {
		wlr_log(WLR_ERROR, "Unexpected data in field parser: %s=\"%s\"",
			nodename, content);
	}
}

bool
osd_field_validate(struct window_switcher_field *field)
{
	if (field->content == LAB_FIELD_NONE) {
		wlr_log(WLR_ERROR, "Invalid OSD field: no content set");
		return false;
	}
	if (field->content == LAB_FIELD_CUSTOM && !field->format) {
		wlr_log(WLR_ERROR, "Invalid OSD field: custom without format");
		return false;
	}
	if (!field->width) {
		wlr_log(WLR_ERROR, "Invalid OSD field: no width");
		return false;
	}
	return true;
}

void
osd_field_get_content(struct window_switcher_field *field,
		struct buf *buf, struct view *view)
{
	if (field->content == LAB_FIELD_NONE) {
		wlr_log(WLR_ERROR, "Invalid window switcher field type");
		return;
	}
	assert(field->content < LAB_FIELD_COUNT && field_handlers[field->content].fn);

	field_handlers[field->content].fn(buf, view, field->format);
}

void
osd_field_free(struct window_switcher_field *field)
{
	zfree(field->format);
	zfree(field);
}
