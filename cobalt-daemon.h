/*
 * cobalt-daemon
 *
 * This is a little C language module to include into your Linux (or
 * other POSIX compatible) applications, which will allow to easily
 * drop root privileges and become a daemon service process running
 * under a special user or *nobody*.
 *
 * See the provided example program for a quick start.
 *
 * https://github.com/0xebef/cobalt-daemon
 *
 * License: LGPLv3 or later
 *
 * Copyright (c) 2016, 0xebef
 */

#ifndef COBALT_DAEMON_H_INCLUDED
#define COBALT_DAEMON_H_INCLUDED

#if defined(__cplusplus)
extern "C" {
#endif

#include <limits.h>

/*
 * configuration
 */

/* umask of the daemon, see umask(2) manual page  */
#define DAEMON_UMASK        (S_IWGRP | S_IWOTH) /* umask 022 */

/* syslog facility, see syslog(3) manual page */
#define DAEMON_LOG_FACILITY (LOG_USER)

/*
 * external variable declarations
 */

extern int pidfile_fd;
extern char pidfile_name[PATH_MAX];

/*
 * function declarations
 */

int cobalt_daemon(const char *username, const char *daemonname);

#if defined(__cplusplus)
}
#endif

#endif /* COBALT_DAEMON_H_INCLUDED */
