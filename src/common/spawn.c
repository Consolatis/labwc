// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "common/spawn.h"
#include "common/fd-util.h"

static void
reset_signals_and_limits(void)
{
	restore_nofile_limit();

	sigset_t set;
	sigemptyset(&set);
	sigprocmask(SIG_SETMASK, &set, NULL);

	/* Restore ignored signals */
	signal(SIGPIPE, SIG_DFL);
}

static bool
set_cloexec(int fd)
{
	int flags = fcntl(fd, F_GETFD);
	if (flags == -1) {
		wlr_log_errno(WLR_ERROR,
			"Unable to set the CLOEXEC flag: fnctl failed");
		return false;
	}
	flags = flags | FD_CLOEXEC;
	if (fcntl(fd, F_SETFD, flags) == -1) {
		wlr_log_errno(WLR_ERROR,
			"Unable to set the CLOEXEC flag: fnctl failed");
		return false;
	}
	return true;
}

void
spawn_async_no_shell(char const *command)
{
	GError *err = NULL;
	gchar **argv = NULL;

	assert(command);

	/* Use glib's shell-parse to mimic Openbox's behaviour */
	g_shell_parse_argv((gchar *)command, NULL, &argv, &err);
	if (err) {
		g_message("%s", err->message);
		g_error_free(err);
		return;
	}

	/*
	 * Avoid zombie processes by using a double-fork, whereby the
	 * grandchild becomes orphaned & the responsibility of the OS.
	 */
	pid_t child = 0, grandchild = 0;

	child = fork();
	switch (child) {
	case -1:
		wlr_log(WLR_ERROR, "unable to fork()");
		goto out;
	case 0:
		reset_signals_and_limits();

		setsid();
		grandchild = fork();
		if (grandchild == 0) {
			execvp(argv[0], argv);
			_exit(0);
		} else if (grandchild < 0) {
			wlr_log(WLR_ERROR, "unable to fork()");
		}
		_exit(0);
	default:
		break;
	}
	waitpid(child, NULL, 0);
out:
	g_strfreev(argv);
}

pid_t
spawn_primary_client(const char *command)
{
	assert(command);

	GError *err = NULL;
	gchar **argv = NULL;

	/* Use glib's shell-parse to mimic Openbox's behaviour */
	g_shell_parse_argv((gchar *)command, NULL, &argv, &err);
	if (err) {
		g_message("%s", err->message);
		g_error_free(err);
		return -1;
	}

	pid_t child = fork();
	switch (child) {
	case -1:
		wlr_log_errno(WLR_ERROR, "Failed to fork");
		g_strfreev(argv);
		return -1;
	case 0:
		/* child */
		close(STDIN_FILENO);
		reset_signals_and_limits();
		execvp(argv[0], argv);
		wlr_log_errno(WLR_ERROR, "Failed to execute primary client %s", command);
		_exit(1);
	default:
		g_strfreev(argv);
		return child;
	}
}

static inline void
replace_fd(int target, int from, int alternative)
{
	if (from >= 0) {
		dup2(from, target);
	} else if (alternative >= 0) {
		dup2(alternative, target);
	} else {
		close(target);
	}
}

static pid_t
spawn_pipe(const char *command, int stdin, int stdout, int stderr)
{
	assert(command);

	pid_t pid = fork();
	if (pid < 0) {
		wlr_log(WLR_ERROR, "unable to fork()");
		return pid;
	}

	if (pid == 0) {
		/* child */
		reset_signals_and_limits();

		int dev_null = open("/dev/null", O_RDWR);
		replace_fd(STDIN_FILENO, stdin, dev_null);
		replace_fd(STDOUT_FILENO, stdout, dev_null);
		replace_fd(STDERR_FILENO, stderr, dev_null);
		/*
		 * Technically we could end up closing the
		 * same fd twice so we just ignore errors.
		 */
		if (dev_null >= 0) {
			close(dev_null);
		}
		if (stdin >= 0) {
			close(stdin);
		}
		if (stdout >= 0) {
			close(stdout);
		}
		if (stderr >= 0) {
			close(stderr);
		}

		execl("/bin/sh", "sh", "-c", command, NULL);
		/*
		 * Our stderr points to somewhere or is closed
		 * at this point so logging is pretty useless.
		 */
		_exit(1);
	}

	return pid;
}

pid_t
spawn_pipe_reader(const char *command, int *pipe_fd)
{
	int pipe_rw[2];
	if (pipe(pipe_rw) != 0) {
		wlr_log(WLR_ERROR, "unable to pipe()");
		return -1;
	}

	/*
	 * Prevent leaking the read end of the pipe to this and
	 * further children forked during the lifetime of the descriptor.
	 */
	if (!set_cloexec(pipe_rw[0])) {
		close(pipe_rw[0]);
		close(pipe_rw[1]);
		return -1;
	}

	pid_t client = spawn_pipe(command, -1, pipe_rw[1], -1);
	close(pipe_rw[1]);

	if (client < 0) {
		close(pipe_rw[0]);
	} else {
		*pipe_fd = pipe_rw[0];
	}
	return client;
}

pid_t
spawn_pipe_writer(const char *command, int *pipe_fd)
{
	int pipe_rw[2];
	if (pipe(pipe_rw) != 0) {
		wlr_log(WLR_ERROR, "unable to pipe()");
		return -1;
	}

	/*
	 * Prevent leaking the write end of the pipe to this and
	 * further children forked during the lifetime of the descriptor.
	 */
	if (!set_cloexec(pipe_rw[1])) {
		close(pipe_rw[0]);
		close(pipe_rw[1]);
		return -1;
	}

	pid_t client = spawn_pipe(command, pipe_rw[0], -1, -1);
	close(pipe_rw[0]);

	if (client < 0) {
		close(pipe_rw[1]);
	} else {
		*pipe_fd = pipe_rw[1];
	}
	return client;
}
