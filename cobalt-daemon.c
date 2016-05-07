/*
 * cobalt-daemon
 *
 * This is a little C language module to include into your Linux (or
 * other POSIX compatible) applications, which will allow to easily
 * drop root privileges and become a daemon service process running
 * under a special user or "nobody".
 *
 * See the provided example program for a quick start.
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
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "cobalt-daemon.h"

/* the directory where PID file will be created and locked */
#define PID_DIRECTORY            "/var/run"

/*
 * the maximum number of file descriptors exptected to have opened
 * before calling the cobalt_daemon function
 */
#define MAX_OPEN_FDS             (64)

/* the directory separator character */
#define DIRECTORY_SEPARATOR      '/'

/*
 * these variable are exposed to the external world using "extern"
 * declarations in the header file
 */
int pidfile_fd;
char pidfile_name[PATH_MAX] = {0};

int cobalt_daemon(const char *username, const char *daemonname)
{
    int i;
    int fd;
    uid_t uid;
    gid_t gid;
    pid_t pid;
    struct passwd pwd;
    struct passwd *pwdp;
    long buf_sizel;
    size_t buf_size;
    char *buf;
    char pid_buf[32U]; /* max. unsigned 64-bit value is 20 chars */
    struct stat st;
    size_t n;
    struct flock ex_flock;

    if (!username) {
        fprintf(stderr, "error: invalid username\n");
        return -1;
    }

    n = strlen(username);
    if (n == 0U) {
        fprintf(stderr, "error: empty username\n");
        return -1;
    }

    if (!daemonname) {
        fprintf(stderr, "error: invalid daemonname\n");
        return -1;
    }

    n = strlen(daemonname);
    if (n == 0U) {
        fprintf(stderr, "error: empty daemonname\n");
        return -1;
    }

    if (geteuid() != 0U) {
        fprintf(stderr, "error: not running with root privileges\n");
        return -1;
    }

    buf_sizel = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (buf_sizel <= 0) {
        /* value was indeterminate */
        buf_size = 16384U; /* getpwnam(3): should be more than enough */
    } else {
        buf_size = (size_t)buf_sizel;
    }

    buf = malloc(buf_size);
    if (buf == NULL) {
        fprintf(stderr, "error: can not allocate memory\n");
        return -1;
    }

    if (getpwnam_r(username, &pwd, buf, buf_size, &pwdp) != 0) {
        fprintf(stderr, "error: can not check whether the '%s' user "
                "exists\n", username);
        free(buf);
        return -1;
    } else if (pwdp == NULL) {
        fprintf(stderr, "error: can not find the '%s' user "
                "in the system\n", username);
        free(buf);
        return -1;
    }

    free(buf);

    uid = pwdp->pw_uid;
    gid = pwdp->pw_gid;

    if (uid < 1U) {
        fprintf(stderr, "error: it is required to specify a non-root "
                "user for daemonizing\n");
        return -1;
    }

    i = snprintf(pidfile_name, sizeof pidfile_name, "%s",
            PID_DIRECTORY);
    if (i >= (int)sizeof pidfile_name) {
        fprintf(stderr, "error: PID_DIRECTORY is too long\n");
        return -1;
    } else if (i <= 0) {
        fprintf(stderr, "error: PID_DIRECTORY is invalid\n");
        return -1;
    }

    n = strlen(pidfile_name);

    if (n >= (int)PATH_MAX) {
        fprintf(stderr, "error: PID_DIRECTORY is too long\n");
        return -1;
    }

    if (pidfile_name[n - 1U] != DIRECTORY_SEPARATOR) {
        pidfile_name[n++] = DIRECTORY_SEPARATOR;
    }

    pidfile_name[n] = '\0';

    /* create the pid directory if it does not exist */
    if (stat(pidfile_name, &st) == -1) {
        if (mkdir(pidfile_name, 0755) == -1) {
            fprintf(stderr, "error: can not create the "
                    "pid directory: '%s'\n", pidfile_name);
            return -1;
        }
    }

    /* set a new owner for the pid directory */
    if (chown(pidfile_name, uid, gid) == -1) {
        fprintf(stderr, "error: can not chown the pid directory "
                "'%s'\n", pidfile_name);
        return -1;
    }

    /* generate the full pidfile name */
    if (snprintf(&pidfile_name[n], PATH_MAX - n, "%s.pid",
                daemonname) >= (int)(PATH_MAX - n)) {
        fprintf(stderr, "error: pid file path is too long\n");
        return -1;
    }

    /* set the working directory to the root directory */
    if (chdir("/") == -1) {
        fprintf(stderr, "error: can not change directory to /\n");
        return -1;
    }

    /* fork the new process */
    pid = fork();

    if (pid == -1) {
        fprintf(stderr, "error: can not fork, errno = %d\n", errno);
        return -1;
    }

    if (pid != 0) {
        /* prevent the atexit destructor from deleting the pidfile */
        pidfile_name[0U] = '\0';

        /* exit the parent process */
        exit(EXIT_SUCCESS);
    }

    /* try to create the pidfile */
    pidfile_fd = open(pidfile_name, O_RDWR | O_CREAT | O_EXCL,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (pidfile_fd == -1) {
        /* perhaps the pidfile already exists, try to open it */
        pidfile_fd = open(pidfile_name, O_RDONLY);

        if (pidfile_fd == -1) {
            fprintf(stderr, "error: can not get the pidfile '%s'\n",
                    pidfile_name);
            return -1;
        }

        /* find out what that pid is */
        i = read(pidfile_fd, pid_buf, sizeof pid_buf);
        if (i > 0) {
            if (pid_buf[(size_t)i - 1U] == '\n') {
                pid_buf[(size_t)i - 1U] = '\0'; /* trim */
            }

            pid = strtoull(pid_buf, NULL, 10);

            /*
             * find out whether that process is running as the signal 0
             * in kill(2) doesn't send a signal, but still performs the
             * necessary error checkings
             */
            if (kill(pid, 0) == 0) {
                fprintf(stderr, "error: pidfile '%s' detected and it "
                        "may be owned by the process with pid %d\n",
                        pidfile_name, pid);
            } else if (errno == ESRCH) {
                /* non-existent process */
                fprintf(stderr, "warning: pidfile '%s' was detected "
                        "and it is owned by a non-existent process %d, "
                        "we will try to delete the pidfile\n",
                        pidfile_name, pid);

                close(pidfile_fd);
                pidfile_fd = -1;

                if (unlink(pidfile_name) != -1) {
                    return cobalt_daemon(username, daemonname);
                } else {
                    fprintf(stderr, "error: can't delete the "
                        "pidfile '%s'\n", pidfile_name);
                }
            } else {
                fprintf(stderr, "error: can't get the pidfile '%s'\n",
                        pidfile_name);
            }
        } else {
            fprintf(stderr, "error: can't read the pidfile '%s'\n",
                    pidfile_name);
        }

        if (pidfile_fd != -1) {
            close(pidfile_fd);
        }

        return -1;
    }

    /* set the pidfile's ownership */
    if (fchown(pidfile_fd, uid, gid) == -1) {
        fprintf(stderr, "error: can't chown the pidfile '%s'\n",
                pidfile_name);
        return -1;
    }

    ex_flock.l_type = F_WRLCK;               /* exclusive write lock */
    ex_flock.l_whence = SEEK_SET;
    ex_flock.l_len = ex_flock.l_start = 0;   /* the whole file */
    ex_flock.l_pid = 0;
    if (fcntl(pidfile_fd, F_SETLK, &ex_flock) == -1) {
        fprintf(stderr, "error: can't set a lock on the pidfile\n");
        close(pidfile_fd);
        return -1;
    }

    if (ftruncate(pidfile_fd, 0) == -1) {
        close(pidfile_fd);
        fprintf(stderr, "error: can't truncate the pidfile\n");
        return -1;
    }

    i = snprintf(pid_buf, sizeof pid_buf, "%d\n", getpid());
    if (i >= (int)sizeof pid_buf) {
        fprintf(stderr, "error: unexpectedly big pid\n");
        close(pidfile_fd);
        return -1;
    } else if (i > 0) {
        if (write(pidfile_fd, pid_buf, i) != (ssize_t)i) {
            fprintf(stderr, "error: can't write into the pidfile\n");
            close(pidfile_fd);
            return -1;
        }
    } else {
        fprintf(stderr, "error: can't sprintf() the getpid()\n");
        close(pidfile_fd);
        return -1;
    }

    /* create new session and process group */
    if (setsid() == -1) {
        close(pidfile_fd);
        return -1;
    }

    /* set the umask */
    umask(DAEMON_UMASK);

    /* open syslog */
    openlog(daemonname, LOG_PID | LOG_CONS, DAEMON_LOG_FACILITY);

    /* drop group privileges */
    if (setresgid(gid, gid, gid) == -1) {
        fprintf(stderr, "error: setresgid failed\n");
        close(pidfile_fd);
        return -1;
    }

    /* drop user privileges */
    if (setresuid(uid, uid, uid) == -1) {
        fprintf(stderr, "error: setresuid failed\n");
        close(pidfile_fd);
        return -1;
    }

    /* close open file descriptors */
    for (i = 0; i <= MAX_OPEN_FDS; i++) {
        if (i != pidfile_fd) {
            close(i);
        }
    }

    /*
     * redirect 0, 1 and 2 file descriptors into /dev/null
     */

    /* stdin */
    fd = open("/dev/null", O_RDWR);
    if (fd != 0) {
        if (fd) {
            close(fd);
        }
        fprintf(stderr, "error: can not redirect stdin\n");
        close(pidfile_fd);
        return -1;
    }

    /* stdout */
    fd = dup(0);
    if (fd != 1) {
        if (fd) {
            close(fd);
        }
        fprintf(stderr, "error: can not redirect stdout\n");
        close(pidfile_fd);
        return -1;
    }

    /* stderror */
    fd = dup(0);
    if (fd != 2) {
        if (fd) {
            close(fd);
        }
        fprintf(stderr, "error: can not redirect stderror\n");
        close(pidfile_fd);
        return -1;
    }

    return 0;
}
