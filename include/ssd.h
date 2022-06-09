/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __LABWC_SSD_H
#define __LABWC_SSD_H

/* TODO: Move to theme->ssd_button_width */
#define BUTTON_WIDTH 26

/*
 * Sequence these according to the order they should be processed for
 * press and hover events. Bear in mind that some of their respective
 * interactive areas overlap, so for example buttons need to come before title.
 */
enum ssd_part_type {
	LAB_SSD_NONE = 0,
	LAB_SSD_BUTTON_CLOSE,
	LAB_SSD_BUTTON_MAXIMIZE,
	LAB_SSD_BUTTON_ICONIFY,
	LAB_SSD_BUTTON_WINDOW_MENU,
	LAB_SSD_PART_TITLEBAR,
	LAB_SSD_PART_TITLE,
	LAB_SSD_PART_CORNER_TOP_LEFT,
	LAB_SSD_PART_CORNER_TOP_RIGHT,
	LAB_SSD_PART_CORNER_BOTTOM_RIGHT,
	LAB_SSD_PART_CORNER_BOTTOM_LEFT,
	LAB_SSD_PART_TOP,
	LAB_SSD_PART_RIGHT,
	LAB_SSD_PART_BOTTOM,
	LAB_SSD_PART_LEFT,
	LAB_SSD_CLIENT,
	LAB_SSD_FRAME,
	LAB_SSD_ROOT,
	LAB_SSD_MENU,
	LAB_SSD_OSD,
	LAB_SSD_LAYER_SURFACE,
	LAB_SSD_UNMANAGED,
	LAB_SSD_END_MARKER
};

/* Forward declare arguments */
struct view;
struct wl_list;
struct wlr_box;
struct wlr_scene_node;
struct wlr_scene_tree;

struct ssd_sub_tree {
	struct wlr_scene_tree *tree;
	struct wl_list parts; /* ssd_part::link */
};

struct ssd_state_title_width {
	int width;
	bool truncated;
};

struct ssd {
	bool enabled;
	struct wlr_scene_tree *tree;

	/*
	 * Cache for current values.
	 * Used to detect actual changes so we
	 * don't update things we don't have to.
	 */
	struct {
		int x;
		int y;
		int width;
		int height;
		struct ssd_state_title {
			char *text;
			struct ssd_state_title_width active;
			struct ssd_state_title_width inactive;
		} title;
	} state;

	/* An invisble area around the view which allows resizing */
	struct ssd_sub_tree extents;

	/* The top of the view, containing buttons, title, .. */
	struct {
		struct ssd_sub_tree active;
		struct ssd_sub_tree inactive;
	} titlebar;

	/* Borders allow resizing as well */
	struct {
		struct ssd_sub_tree active;
		struct ssd_sub_tree inactive;
	} border;
};

struct ssd_hover_state {
	struct view *view;
	enum ssd_part_type type;
	struct wlr_scene_node *node;
};

/* Public SSD API */
void ssd_create(struct view *view);
void ssd_hide(struct view *view);
void ssd_set_active(struct view *view);
void ssd_update_title(struct view *view);
void ssd_update_geometry(struct view *view);
void ssd_reload(struct view *view);
void ssd_destroy(struct view *view);
/* Returns hover overlay node so it can be disabled later on */
struct wlr_scene_node *ssd_button_hover_enable(
	struct view *view, enum ssd_part_type type);

/* Public SSD helpers */
/* TODO: clean up / move / update */
enum ssd_part_type ssd_at(struct view *view, double lx, double ly);
enum ssd_part_type ssd_get_part_type(
	struct view *view, struct wlr_scene_node *node);
uint32_t ssd_resize_edges(enum ssd_part_type type);
bool ssd_is_button(enum ssd_part_type type);
bool ssd_part_contains(enum ssd_part_type whole, enum ssd_part_type candidate);
struct border ssd_thickness(struct view *view);
struct wlr_box ssd_max_extents(struct view *view);

#endif /* __LABWC_SSD_H */
