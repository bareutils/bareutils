/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define STATUS_SIZE 20
typedef enum {
    STATE_DOWN = 0,
    STATE_RUN  = 1,
    STATE_FINISH = 2,
} svc_state_t;
typedef enum {
    WANT_DOWN = 'd',
    WANT_UP   = 'u',
} svc_want_t;
typedef struct {
    char    svcdir[4080];
    char    supdir[4096];
    char    ctlpath[4096];
    char    okpath[4096];
    char    lockpath[4096];
    char    statpath[4096];
    char    logdir[4096];
    int     ctlfd;
    int     okfd;
    pid_t   pid;
    pid_t   log_pid;
    int     log_pipe[2];
    svc_state_t state;
    svc_want_t  want;
    bool    paused;
    bool    once;
    time_t  start_time;
    int     last_exit;
    int     last_sig;
    bool    term_sig;
} svc_t;
static volatile sig_atomic_t g_sigchld = 0;
static volatile sig_atomic_t g_sigterm = 0;
static void on_sigchld(int s) { (void)s; g_sigchld = 1; }
static void on_sigterm(int s) { (void)s; g_sigterm = 1; }
static bool flag_help = false;
static void write_status(svc_t *s) {
    uint8_t buf[STATUS_SIZE];
    memset(buf, 0, sizeof(buf));
    uint64_t tai = (uint64_t)s->start_time + 4611686018427387914ULL;
    buf[0]  = (tai >> 56) & 0xff;
    buf[1]  = (tai >> 48) & 0xff;
    buf[2]  = (tai >> 40) & 0xff;
    buf[3]  = (tai >> 32) & 0xff;
    buf[4]  = (tai >> 24) & 0xff;
    buf[5]  = (tai >> 16) & 0xff;
    buf[6]  = (tai >>  8) & 0xff;
    buf[7]  =  tai        & 0xff;
    buf[8]  = 0; buf[9]  = 0; buf[10] = 0; buf[11] = 0;
    pid_t pid = s->pid > 0 ? s->pid : 0;
    buf[12] = (pid >> 24) & 0xff;
    buf[13] = (pid >> 16) & 0xff;
    buf[14] = (pid >>  8) & 0xff;
    buf[15] =  pid        & 0xff;
    buf[16] = s->paused ? 1 : 0;
    buf[17] = (uint8_t)s->want;
    buf[18] = s->term_sig ? 1 : 0;
    buf[19] = (s->state == STATE_RUN) ? 1 : (s->state == STATE_FINISH) ? 2 : 0;
    char tmppath[4096];
    snprintf(tmppath, sizeof(tmppath), "%s/status.tmp", s->supdir);
    int fd = open(tmppath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    (void)write(fd, buf, STATUS_SIZE);
    close(fd);
    rename(tmppath, s->statpath);
}
static void make_supervise_fds(svc_t *s) {
    struct stat st;
    if (stat(s->ctlpath, &st) < 0 || !S_ISFIFO(st.st_mode)) {
        unlink(s->ctlpath);
        mkfifo(s->ctlpath, 0600);
    }
    if (stat(s->okpath, &st) < 0 || !S_ISFIFO(st.st_mode)) {
        unlink(s->okpath);
        mkfifo(s->okpath, 0600);
    }
    s->ctlfd = open(s->ctlpath, O_RDWR | O_NONBLOCK);
    s->okfd  = open(s->okpath,  O_RDWR | O_NONBLOCK);
    int lfd = open(s->lockpath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (lfd >= 0) {
        char pidbuf[24];
        int n = snprintf(pidbuf, sizeof(pidbuf), "%d\n", (int)getpid());
        (void)write(lfd, pidbuf, (size_t)n);
        close(lfd);
    }
}
static void spawn_log(svc_t *s) {
    char logrun[4096];
    snprintf(logrun, sizeof(logrun), "%s/run", s->logdir);
    struct stat st;
    if (stat(logrun, &st) < 0 || !(st.st_mode & S_IXUSR)) return;
    if (pipe(s->log_pipe) < 0) return;
    pid_t pid = fork();
    if (pid < 0) { close(s->log_pipe[0]); close(s->log_pipe[1]); return; }
    if (pid == 0) {
        dup2(s->log_pipe[0], STDIN_FILENO);
        close(s->log_pipe[0]);
        close(s->log_pipe[1]);
        if (chdir(s->logdir) < 0) _exit(111);
        char *argv[] = { logrun, NULL };
        execv(logrun, argv);
        _exit(111);
    }
    close(s->log_pipe[0]);
    s->log_pid = pid;
}
static void svc_start(svc_t *s) {
    if (s->pid > 0) return;
    char runpath[4096];
    snprintf(runpath, sizeof(runpath), "%s/run", s->svcdir);
    struct stat st;
    if (stat(runpath, &st) < 0 || !(st.st_mode & S_IXUSR)) return;
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        if (chdir(s->svcdir) < 0) _exit(111);
        if (s->log_pid > 0) {
            dup2(s->log_pipe[1], STDOUT_FILENO);
            close(s->log_pipe[1]);
        }
        setsid();
        char *argv[] = { runpath, NULL };
        execv(runpath, argv);
        char msg[4096];
        snprintf(msg, sizeof(msg), "msv: exec %s: %s\n", runpath, strerror(errno));
        (void)write(STDERR_FILENO, msg, strlen(msg));
        _exit(111);
    }
    s->pid = pid;
    s->state = STATE_RUN;
    s->start_time = time(NULL);
    s->term_sig = false;
    write_status(s);
}
static void svc_finish(svc_t *s, int exitcode, int exitsig) {
    s->state = STATE_FINISH;
    s->start_time = time(NULL);
    write_status(s);
    char finpath[4096];
    snprintf(finpath, sizeof(finpath), "%s/finish", s->svcdir);
    struct stat st;
    if (stat(finpath, &st) < 0 || !(st.st_mode & S_IXUSR)) return;
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        if (chdir(s->svcdir) < 0) _exit(111);
        char code[12], sig[12];
        snprintf(code, sizeof(code), "%d", exitcode);
        snprintf(sig,  sizeof(sig),  "%d", exitsig);
        char *argv[] = { finpath, code, sig, NULL };
        execv(finpath, argv);
        _exit(111);
    }
    int fst;
    while (waitpid(pid, &fst, 0) < 0 && errno == EINTR);
}
static void handle_ctl(svc_t *s) {
    char buf[32];
    ssize_t n = read(s->ctlfd, buf, sizeof(buf));
    if (n <= 0) return;
    for (ssize_t i = 0; i < n; i++) {
        char c = buf[i];
        switch (c) {
        case 'u':
            s->want = WANT_UP;
            s->once = false;
            if (s->pid <= 0) svc_start(s);
            else if (s->paused) { s->paused = false; kill(s->pid, SIGCONT); }
            break;
        case 'd':
            s->want = WANT_DOWN;
            s->once = false;
            if (s->pid > 0 && !s->paused) kill(s->pid, SIGTERM);
            else if (s->pid > 0 && s->paused) { kill(s->pid, SIGCONT); kill(s->pid, SIGTERM); }
            s->term_sig = true;
            break;
        case 'o':
            s->once = true;
            s->want = WANT_UP;
            if (s->pid <= 0) svc_start(s);
            break;
        case 'p':
            if (s->pid > 0 && !s->paused) { s->paused = true; kill(s->pid, SIGSTOP); }
            break;
        case 'c':
            if (s->pid > 0 && s->paused)  { s->paused = false; kill(s->pid, SIGCONT); }
            break;
        case 'h':
            if (s->pid > 0) kill(s->pid, SIGHUP);
            break;
        case 'a':
            if (s->pid > 0) kill(s->pid, SIGALRM);
            break;
        case 'i':
            if (s->pid > 0) kill(s->pid, SIGINT);
            break;
        case 'q':
            if (s->pid > 0) kill(s->pid, SIGQUIT);
            break;
        case '1':
            if (s->pid > 0) kill(s->pid, SIGUSR1);
            break;
        case '2':
            if (s->pid > 0) kill(s->pid, SIGUSR2);
            break;
        case 't':
            if (s->pid > 0) kill(s->pid, SIGTERM);
            break;
        case 'k':
            if (s->pid > 0) kill(s->pid, SIGKILL);
            break;
        case 'x':
            g_sigterm = 1;
            break;
        default:
            break;
        }
    }
    write_status(s);
}
static void reap_svc(svc_t *s) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == s->log_pid) {
            s->log_pid = -1;
            continue;
        }
        if (pid != s->pid) continue;
        int code = WIFEXITED(status)  ? WEXITSTATUS(status) : -1;
        int sig  = WIFSIGNALED(status) ? WTERMSIG(status)   : -1;
        s->last_exit = code;
        s->last_sig  = sig;
        s->pid = -1;
        svc_finish(s, code, sig);
        s->state = STATE_DOWN;
        s->start_time = time(NULL);
        write_status(s);
        if (!g_sigterm && s->want == WANT_UP && !s->once) {
            struct timespec ts = { 1, 0 };
            nanosleep(&ts, NULL);
            svc_start(s);
        }
    }
}
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "msv", "msv [options] <svcdir>",
                  "Supervise a single runit-style service (like runsv).");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }
    if (cli.npositional < 1) bare_die("msv", "usage: msv <svcdir>");
    svc_t s;
    memset(&s, 0, sizeof(s));
    s.pid     = -1;
    s.log_pid = -1;
    s.want    = WANT_UP;
    s.state   = STATE_DOWN;
    s.log_pipe[0] = -1;
    s.log_pipe[1] = -1;
    snprintf(s.svcdir,   sizeof(s.svcdir),   "%s", cli.positional[0]);
    snprintf(s.supdir,   sizeof(s.supdir),   "%s/supervise",          s.svcdir);
    snprintf(s.ctlpath,  sizeof(s.ctlpath),  "%s/supervise/control",  s.svcdir);
    snprintf(s.okpath,   sizeof(s.okpath),   "%s/supervise/ok",       s.svcdir);
    snprintf(s.lockpath, sizeof(s.lockpath), "%s/supervise/lock",     s.svcdir);
    snprintf(s.statpath, sizeof(s.statpath), "%s/supervise/status",   s.svcdir);
    snprintf(s.logdir,   sizeof(s.logdir),   "%s/log",                s.svcdir);
    mkdir(s.supdir, 0700);
    char downpath[4096];
    snprintf(downpath, sizeof(downpath), "%s/down", s.svcdir);
    struct stat downst;
    if (stat(downpath, &downst) == 0) s.want = WANT_DOWN;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    sa.sa_handler = on_sigchld; sigaction(SIGCHLD, &sa, NULL);
    sa.sa_flags   = SA_RESTART;
    sa.sa_handler = on_sigterm; sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = SIG_IGN;    sigaction(SIGHUP,  &sa, NULL);
    sa.sa_handler = SIG_IGN;    sigaction(SIGPIPE, &sa, NULL);
    make_supervise_fds(&s);
    spawn_log(&s);
    s.start_time = time(NULL);
    write_status(&s);
    svc_start(&s);
    for (;;) {
        if (g_sigchld) { g_sigchld = 0; reap_svc(&s); }
        if (g_sigterm) {
            if (s.pid > 0) { kill(s.pid, SIGTERM); s.want = WANT_DOWN; }
            break;
        }
        handle_ctl(&s);
        struct timespec ts = { 0, 50000000L };
        nanosleep(&ts, NULL);
    }
    int waited = 0;
    while (s.pid > 0 && waited < 5) {
        struct timespec ts = { 1, 0 };
        nanosleep(&ts, NULL);
        reap_svc(&s);
        waited++;
    }
    if (s.pid > 0) kill(s.pid, SIGKILL);
    if (s.log_pid > 0) {
        close(s.log_pipe[1]);
        kill(s.log_pid, SIGTERM);
        waitpid(s.log_pid, NULL, 0);
    }
    if (s.ctlfd >= 0) close(s.ctlfd);
    if (s.okfd  >= 0) close(s.okfd);
    unlink(s.lockpath);
    s.pid   = -1;
    s.state = STATE_DOWN;
    write_status(&s);
    bare_cli_free(&cli);
    return 0;
}
