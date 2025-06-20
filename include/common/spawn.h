/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SPAWN_H
#define LABWC_SPAWN_H

#include <sys/types.h>

/**
 * spawn_primary_client - execute asynchronously
 * @command: command to be executed
 */
pid_t spawn_primary_client(const char *command);

/**
 * spawn_async_no_shell - execute asynchronously
 * @command: command to be executed
 */
void spawn_async_no_shell(char const *command);

/**
 * spawn_pipe_reader - execute asynchronously
 * @command: command to be executed
 * @pipe_fd: set to the read end of a pipe
 *           connected to stdout of the command
 *
 * Notes:
 * The returned pid_t is being waited for
 * in the global SIGCHLD handler in server.c.
 *
 * The pipe_fd has to be closed by the caller.
 */
pid_t spawn_pipe_reader(const char *command, int *pipe_fd);

/**
 * spawn_pipe_writer - execute asynchronously
 * @command: command to be executed
 * @pipe_fd: set to the write end of a pipe
 *           connected to stdin of the command
 *
 * Notes:
 * The returned pid_t is being waited for
 * in the global SIGCHLD handler in server.c.
 *
 * The pipe_fd has to be closed by the caller.
 */
pid_t spawn_pipe_writer(const char *command, int *pipe_fd);

#endif /* LABWC_SPAWN_H */
