/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define MAXSVS 256
#define SCAN_INTERVAL 5
typedef struct {
    char path[4080];
    pid_t pid;
} svc_t;
static svc_t svcs[MAXSVS];
static int nsvc = 0;
static volatile sig_atomic_t g_rescan  = 1;
static volatile sig_atomic_t g_sigchld = 0;
static volatile sig_atomic_t g_quit    = 0;
static bool flag_help = false;
static void on_sighup(int s)  { (void)s; g_rescan = 1; }
static void on_sigchld(int s) { (void)s; g_sigchld = 1; }
static void on_sigterm(int s) { (void)s; g_quit = 1; }
static int svc_find(const char *path) {
    for (int i = 0; i < nsvc; i++)
        if (strcmp(svcs[i].path, path) == 0) return i;
    return -1;
}
static void svc_spawn(const char *svcdir, const char *name) {
    if (nsvc >= MAXSVS) return;
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", svcdir, name);
    if (svc_find(path) >= 0) return;
    struct stat st;
    if (stat(path, &st) < 0 || !S_ISDIR(st.st_mode)) return;
    char sv_run[4096];
    snprintf(sv_run, sizeof(sv_run), "%s/run", path);
    if (stat(sv_run, &st) < 0) return;
    char sv_sup[4096];
    snprintf(sv_sup, sizeof(sv_sup), "%s/supervise", path);
    mkdir(sv_sup, 0700);
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        char *exargv[] = { "msv", path, NULL };
        execvp("msv", exargv);
        char msg[4096];
        snprintf(msg, sizeof(msg), "msvd: exec msv %s: %s\n", path, strerror(errno));
        (void)write(STDERR_FILENO, msg, strlen(msg));
        _exit(111);
    }
    snprintf(svcs[nsvc].path, sizeof(svcs[nsvc].path), "%s", path);
    svcs[nsvc].pid = pid;
    nsvc++;
}
static void reap_svcs(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < nsvc; i++) {
            if (svcs[i].pid == pid) {
                svcs[i] = svcs[--nsvc];
                break;
            }
        }
    }
}
static void stop_all(void) {
    for (int i = 0; i < nsvc; i++) {
        if (svcs[i].pid > 0) {
            char ctl[4096];
            snprintf(ctl, sizeof(ctl), "%s/supervise/control", svcs[i].path);
            int fd = open(ctl, O_WRONLY | O_NONBLOCK);
            if (fd >= 0) { (void)write(fd, "dx", 2); close(fd); }
            kill(svcs[i].pid, SIGTERM);
        }
    }
    struct timespec ts = { 5, 0 };
    nanosleep(&ts, NULL);
    for (int i = 0; i < nsvc; i++)
        if (svcs[i].pid > 0) kill(svcs[i].pid, SIGKILL);
    while (waitpid(-1, NULL, 0) > 0);
}
static void scan(const char *svcdir) {
    DIR *d = opendir(svcdir);
    if (!d) return;
    bool seen[MAXSVS];
    memset(seen, 0, sizeof(seen));
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", svcdir, de->d_name);
        struct stat st;
        if (stat(path, &st) < 0 || !S_ISDIR(st.st_mode)) continue;
        int idx = svc_find(path);
        if (idx < 0) {
            svc_spawn(svcdir, de->d_name);
            idx = svc_find(path);
        }
        if (idx >= 0) seen[idx] = true;
    }
    closedir(d);
    for (int i = 0; i < nsvc; i++) {
        if (!seen[i] && svcs[i].pid > 0) {
            char ctl[4096];
            snprintf(ctl, sizeof(ctl), "%s/supervise/control", svcs[i].path);
            int fd = open(ctl, O_WRONLY | O_NONBLOCK);
            if (fd >= 0) { (void)write(fd, "dx", 2); close(fd); }
            kill(svcs[i].pid, SIGTERM);
        }
    }
}
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "msvd", "msvd [options] <svdir>",
                  "Supervise all services in a directory (like runsvdir).");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }
    if (cli.npositional < 1) bare_die("msvd", "usage: msvd <svdir>");
    const char *svcdir = cli.positional[0];
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    sa.sa_handler = on_sigchld; sigaction(SIGCHLD, &sa, NULL);
    sa.sa_flags   = SA_RESTART;
    sa.sa_handler = on_sighup;  sigaction(SIGHUP,  &sa, NULL);
    sa.sa_handler = on_sigterm; sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = SIG_IGN;    sigaction(SIGPIPE, &sa, NULL);
    for (;;) {
        if (g_rescan) { g_rescan = 0; scan(svcdir); }
        if (g_sigchld) { g_sigchld = 0; reap_svcs(); scan(svcdir); }
        if (g_quit) break;
        struct timespec ts = { SCAN_INTERVAL, 0 };
        nanosleep(&ts, NULL);
        g_rescan = 1;
    }
    stop_all();
    bare_cli_free(&cli);
    return 0;
}
