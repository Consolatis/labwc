/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_PLUGINS_H
#define LABWC_PLUGINS_H

struct view;
struct server;

void plugins_init(struct server *server);
void plugins_reconfigure(void);
void plugins_view_title_changed(struct view *view);
void plugins_finish(void);

#endif /* LABWC_PLUGINS_H */
