#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include "common/log.h"
#include "common/spawn.h"

static int nag_fd = -1;

void
_nag_log(enum wlr_log_importance verbosity, const char *format, ...)
{
	if (verbosity < WLR_ERROR) {
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

	//fprintf(stdout, "<error> ");
	//vfprintf(stdout, format, args);
	//fprintf(stdout, "\n");
	va_end(args);
}

void
labnag_show(void)
{
	if (nag_fd < 0) {
		wlr_log(WLR_ERROR, "no labnag running");
		return;
	}
	close(nag_fd);
	nag_fd = -1;
}
