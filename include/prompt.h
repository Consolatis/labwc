#ifndef LABWC_PROMPT_H
#define LABWC_PROMPT_H

struct prompt {
	struct view *view;
	struct server *server;
	struct wl_list *branch_then;  /* struct action->link */
	struct wl_list *branch_else;  /* struct action->link */
	const char *label_prompt;
	const char *label_yes;
	const char *label_no;
};

void prompt_show(struct prompt prompt);

#endif /* LABWC_PROMPT_H */
