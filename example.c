/*
 * cobalt-daemon example
 *
 * This is a little example project to demonstrate the usage of the
 * cobalt-daemon module
 *
 * https://github.com/0xebef/cobalt-daemon
 *
 * License: LGPLv3 or later
 *
 * Copyright (c) 2016, 0xebef
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <syslog.h>
#include "cobalt-daemon.h"

/*
 * the account under which the daemon will be running
 */
#define DAEMON_USERNAME "nobody"

/*
 * a helper macro for logging into syslog
 */
#define LOG(format, ...) do { \
    syslog(DAEMON_LOG_FACILITY | LOG_INFO, format, ## __VA_ARGS__); \
} while(0)

/*
 * the destructor function
 */
static void clean_up(void);

static char *argv0_dup = NULL;

int main(int argc, char *argv[])
{
    char *daemonname;

    /*
     * set up the destructor
     */
    if (atexit(clean_up) != 0) {
        fprintf(stderr, "atexit failed\n");
        exit(EXIT_FAILURE);
    }

    /*
     * get rid of the unused parameter warning
     */
    if (argc < 1) {
        fprintf(stderr, "unexpected argc\n");
        exit(EXIT_FAILURE);
    }

    /*
     * the basename function may modify the original string, so we will
     * get a copy to work with
     */
    argv0_dup = strdup(argv[0U]);
    if (!argv0_dup) {
        fprintf(stderr, "couldn't duplicate the first argument\n");
        exit(EXIT_FAILURE);
    }

    /*
     * get the base name of our own process to use as a daemon name
     */
    daemonname = basename(argv0_dup);
    if (daemonname == NULL) {
        fprintf(stderr, "couldn't get the name of the own process\n");
        exit(EXIT_FAILURE);
    }

    /*
     * become a daemon
     */
    if (cobalt_daemon(DAEMON_USERNAME, daemonname) != 0) {
        fprintf(stderr, "couldn't become a daemon\n");
        exit(EXIT_FAILURE);
    }

    LOG("%s daemon started", daemonname);

    /*
     * do something useful here
     */

    LOG("exiting");

    return EXIT_SUCCESS;
}

static void clean_up(void)
{
    if (pidfile_fd != -1) {
        close(pidfile_fd);
    }

    if (pidfile_name[0U] != '\0') {
        unlink(pidfile_name);
    }

    closelog();

    if (argv0_dup) {
        free(argv0_dup);
    }
}
