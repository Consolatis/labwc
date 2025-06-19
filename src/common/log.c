#include <stdio.h>
#include "common/log.h"
#include "common/spawn.h"

static pid_t nag_pid;

void
_nag_log(enum wlr_log_importance verbosity, const char *format, ...)
{
	if (verbosity < WLR_ERROR) {
		return;
	}
	if (!nag_pid) {
		
	}
	va_list args;
	va_start(args, format);
	fprintf(stdout, "<error> ");
	vfprintf(stdout, format, args);
	fprintf(stdout, "\n");
	va_end(args);
}

