// SPDX-License-Identifier: GPL-2.0-only

#include "common/scene-helpers.h"
#include <assert.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include <wlr/util/transform.h>
#include "magnifier.h"
#include "output.h"

struct wlr_surface *
lab_wlr_surface_from_node(struct wlr_scene_node *node)
{
	struct wlr_scene_buffer *buffer;
	struct wlr_scene_surface *scene_surface;

	if (node && node->type == WLR_SCENE_NODE_BUFFER) {
		buffer = wlr_scene_buffer_from_node(node);
		scene_surface = wlr_scene_surface_try_from_buffer(buffer);
		if (scene_surface) {
			return scene_surface->surface;
		}
	}
	return NULL;
}

struct wlr_scene_node *
lab_wlr_scene_get_prev_node(struct wlr_scene_node *node)
{
	assert(node);
	struct wlr_scene_node *prev;
	prev = wl_container_of(node->link.prev, node, link);
	if (&prev->link == &node->parent->children) {
		return NULL;
	}
	return prev;
}

/*
 * This is a slightly modified copy of scene_output_damage(),
 * required to properly add the magnifier damage to scene_output
 * ->damage_ring and scene_output->pending_commit_damage.
 *
 * The only difference is code style and removal of wlr_output_schedule_frame().
 */
static void
scene_output_damage(struct wlr_scene_output *scene_output,
		const pixman_region32_t *damage)
{
	struct wlr_output *output = scene_output->output;

	pixman_region32_t clipped;
	pixman_region32_init(&clipped);
	pixman_region32_intersect_rect(&clipped, damage, 0, 0, output->width, output->height);

	if (pixman_region32_not_empty(&clipped)) {
		wlr_damage_ring_add(&scene_output->damage_ring, &clipped);
		pixman_region32_union(&scene_output->WLR_PRIVATE.pending_commit_damage,
			&scene_output->WLR_PRIVATE.pending_commit_damage, &clipped);
	}

	pixman_region32_fini(&clipped);
}

/*
 * This is a copy of wlr_scene_output_commit()
 * as it doesn't use the pending state at all.
 */
bool
lab_wlr_scene_output_commit(struct wlr_scene_output *scene_output,
		struct wlr_output_state *state)
{
	assert(scene_output);
	assert(state);
	struct wlr_output *wlr_output = scene_output->output;
	struct output *output = wlr_output->data;
	bool wants_magnification = output_wants_magnification(output);

	/*
	 * FIXME: Regardless of wants_magnification, we are currently adding
	 * damages to next frame when magnifier is shown, which forces
	 * rendering on every output commit and overloads CPU.
	 * We also need to verify the necessity of wants_magnification.
	 */
	if (!wlr_scene_output_needs_frame(scene_output) && !wants_magnification) {
		return true;
	}

	if (!wlr_scene_output_build_state(scene_output, state, NULL)) {
		wlr_log(WLR_ERROR, "Failed to build output state for %s",
			wlr_output->name);
		return false;
	}

	if (state->tearing_page_flip) {
		if (!wlr_output_test_state(wlr_output, state)) {
			state->tearing_page_flip = false;
		}
	}

	struct wlr_box additional_damage = {0};
	if (state->buffer && magnifier_is_enabled()) {
		magnifier_draw(output, state->buffer, &additional_damage);
	}

	bool committed = wlr_output_commit_state(wlr_output, state);
	/*
	 * Handle case where the output state test for tearing succeeded,
	 * but actual commit failed. Retry without tearing.
	 */
	if (!committed && state->tearing_page_flip) {
		state->tearing_page_flip = false;
		committed = wlr_output_commit_state(wlr_output, state);
	}
	if (committed) {
		if (state == &output->pending) {
			wlr_output_state_finish(&output->pending);
			wlr_output_state_init(&output->pending);
		}
	} else {
		wlr_log(WLR_INFO, "Failed to commit output %s",
			wlr_output->name);
		return false;
	}

	if (!wlr_box_empty(&additional_damage)) {
		pixman_region32_t region;
		pixman_region32_init_rect(&region,
			additional_damage.x, additional_damage.y,
			additional_damage.width, additional_damage.height);
		scene_output_damage(scene_output, &region);
		pixman_region32_fini(&region);
	}

	return true;
}

static struct wlr_box
_get_node_geometry(struct wlr_scene_node *node)
{
	struct wlr_box box = { .x = node->x, .y = node->y };
	switch (node->type) {
	case WLR_SCENE_NODE_RECT: {
		struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
		box.width = rect->width;
		box.height = rect->height;
		break;
	}
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);
		if (buffer->dst_width > 0 && buffer->dst_height > 0) {
			box.width = buffer->dst_width;
			box.height = buffer->dst_height;
		} else {
			box.width = buffer->WLR_PRIVATE.buffer_width;
			box.height = buffer->WLR_PRIVATE.buffer_height;
			wlr_output_transform_coords(buffer->transform, &box.width, &box.height);
		}
		break;
	}
	default:
		wlr_log(WLR_ERROR, "_get_node_geometry() called with unsupported node type %u", node->type);
	}
	return box;
}

static void
_get_bounding_box(struct wlr_scene_node *node, pixman_region32_t *region, int x, int y)
{
	if (!node->enabled) {
		return;
	}

	if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each(child, &tree->children, link) {
			_get_bounding_box(child, region, x + node->x, y + node->y);
		}
		return;
	}

	struct wlr_box box = _get_node_geometry(node);
	if (box.width && box.height) {
		pixman_region32_union_rect(region, region,
			x + box.x, y + box.y, box.width, box.height);
	}
}

struct wlr_box
lab_wlr_scene_get_bounding_box(struct wlr_scene_node *node)
{
	if (!node->enabled) {
		return (struct wlr_box) {0};
	}

	if (node->type != WLR_SCENE_NODE_TREE) {
		return _get_node_geometry(node);
	}

	pixman_region32_t region;
	pixman_region32_init(&region);
	_get_bounding_box(node, &region, 0, 0);
	struct pixman_box32 *extents = pixman_region32_extents(&region);
	struct wlr_box box = {
		.x = extents->x1,
		.y = extents->y1,
		.width = extents->x2 - extents->x1,
		.height = extents->y2 - extents->y1,
	};
	pixman_region32_fini(&region);
	return box;
}
