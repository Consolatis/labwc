#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include "common/log.h"
#include "common/spawn.h"

static int nag_fd = -1;
static struct wl_event_loop *loop;
static struct wl_event_source *idle_src;

static void
idle_callback(void *data)
{
	idle_src = NULL;
	close(nag_fd);
	nag_fd = -1;
}

void
nag_log_init(struct wl_event_loop *wl_event_loop)
{
	loop = wl_event_loop;
}

void
_nag_log(enum wlr_log_importance verbosity, const char *format, ...)
{
	if (verbosity < WLR_ERROR) {
		return;
	}
	if (!loop) {
		wlr_log(WLR_INFO, "skipping initial log message");
		return;
	}
	if (nag_fd < 0) {
		pid_t labnag = spawn_pipe_writer(
			"labnag -l -m \"Some errors detected\" --button-dismiss X :", &nag_fd);
		if (labnag < 0) {
			wlr_log(WLR_ERROR, "Failed to spawn labnag");
			return;
		}
		assert(nag_fd >= 0);
		wlr_log(WLR_INFO, "labnag spawned");
	}
	va_list args;
	va_start(args, format);

	if (vdprintf(nag_fd, format, args) < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to write to labnag");
	}
	if (dprintf(nag_fd, "\n") < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to write to labnag");
	}

	va_end(args);

	if (!idle_src) {
		wl_event_loop_add_idle(loop, idle_callback, /*data*/ NULL);
	}
}
