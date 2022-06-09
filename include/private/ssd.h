/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_PRIVATE_SSD_H
#define __LABWC_PRIVATE_SSD_H

#include "ssd.h"

#define BUTTON_COUNT 4
#define EXTENDED_AREA 8

#define FOR_EACH(tmp, ...) \
{ \
	__typeof__(tmp) _x[] = { __VA_ARGS__, NULL }; \
	size_t _i = 0; \
	for ((tmp) = _x[_i]; _i < sizeof(_x) / sizeof(_x[0]) - 1; (tmp) = _x[++_i])

#define FOR_EACH_END }

/* Forward declare arguments not already declared in ssd.h */
struct wlr_buffer;
struct lab_data_buffer;

struct ssd_part {
	enum ssd_part_type type;

	/* Buffer pointer. May be NULL */
	struct lab_data_buffer *buffer;

	/* This part represented in scene graph */
	struct wlr_scene_node *node;

	/* Targeted geometry. May be NULL */
	struct wlr_box *geometry;

	struct wl_list link;
};

/* SSD internal helpers to create various SSD elements */
/* TODO: Replace some common args with a struct */
struct ssd_part *add_scene_part(
	struct wl_list *part_list, enum ssd_part_type type);

struct ssd_part *add_scene_rect(
	struct wl_list *list, enum ssd_part_type type,
	struct wlr_scene_tree *parent, int width, int height, int x, int y,
	float color[4]);

struct ssd_part *add_scene_buffer(
	struct wl_list *list, enum ssd_part_type type,
	struct wlr_scene_tree *parent, struct wlr_buffer *buffer, int x, int y);

struct ssd_part *add_scene_button(
	struct wl_list *part_list, enum ssd_part_type type,
	struct wlr_scene_tree *parent, float *bg_color,
	struct wlr_buffer *icon_buffer, int x);

struct ssd_part *add_scene_button_corner(
	struct wl_list *part_list, enum ssd_part_type type,
	struct wlr_scene_tree *parent, struct wlr_buffer *corner_buffer,
	struct wlr_buffer *icon_buffer, int x);

/* SSD internal helpers */
struct ssd_part *ssd_get_part(
	struct wl_list *part_list, enum ssd_part_type type);
void ssd_destroy_parts(struct wl_list *list);

/* SSD internal API */
void ssd_titlebar_create(struct view *view);
void ssd_titlebar_update(struct view *view);
void ssd_titlebar_destroy(struct view *view);

void ssd_border_create(struct view *view);
void ssd_border_update(struct view *view);
void ssd_border_destroy(struct view *view);

void ssd_extents_create(struct view *view);
void ssd_extents_update(struct view *view);
void ssd_extents_destroy(struct view *view);

#endif /* __LABWC_PRIVATE_SSD_H */
