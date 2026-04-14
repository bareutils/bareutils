/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
static volatile sig_atomic_t g_sigint  = 0;
static volatile sig_atomic_t g_sigterm = 0;
static volatile sig_atomic_t g_sigpwr  = 0;
static volatile sig_atomic_t g_sigchld = 0;
static void on_sigint(int s)  { (void)s; g_sigint  = 1; }
static void on_sigterm(int s) { (void)s; g_sigterm = 1; }
static void on_sigpwr(int s)  { (void)s; g_sigpwr  = 1; }
static void on_sigchld(int s) { (void)s; g_sigchld = 1; }
static volatile pid_t g_runit_pid = -1;
static void reap(void) {
    int st;
    while (waitpid(-1, &st, WNOHANG) > 0);
}
static void run_ctrlaltdel(void) {
    struct stat st;
    if (stat("/etc/runit/ctrlaltdel", &st) < 0 || !(st.st_mode & S_IXUSR)) return;
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        char *argv[] = { "/etc/runit/ctrlaltdel", NULL };
        setsid();
        execv("/etc/runit/ctrlaltdel", argv);
        _exit(111);
    }
}
static void forward_to_runit(void) {
    if (g_runit_pid <= 0) return;
    if (g_sigint)  { run_ctrlaltdel(); kill(g_runit_pid, SIGCONT); kill(g_runit_pid, SIGINT);  g_sigint  = 0; }
    if (g_sigterm) { kill(g_runit_pid, SIGCONT); kill(g_runit_pid, SIGTERM); g_sigterm = 0; }
    if (g_sigpwr)  { kill(g_runit_pid, SIGCONT); kill(g_runit_pid, SIGPWR);  g_sigpwr  = 0; }
}
static void sigmask_set(int how, int *sigs, int n) {
    sigset_t ss;
    sigemptyset(&ss);
    for (int i = 0; i < n; i++) sigaddset(&ss, sigs[i]);
    sigprocmask(how, &ss, NULL);
}
static void do_halt(int cmd) {
    sync();
    reboot(cmd);
    for (;;) pause();
}
static void mount_essential(void) {
    struct { const char *src, *tgt, *fs, *opt; unsigned long fl; } mounts[] = {
        { "proc",     "/proc",    "proc",     NULL,          MS_NODEV|MS_NOSUID|MS_NOEXEC },
        { "sysfs",    "/sys",     "sysfs",    NULL,          MS_NODEV|MS_NOSUID|MS_NOEXEC },
        { "devtmpfs", "/dev",     "devtmpfs", "mode=0755",   MS_NOSUID },
        { "devpts",   "/dev/pts", "devpts",   "mode=0620",   MS_NOSUID|MS_NOEXEC },
        { "tmpfs",    "/dev/shm", "tmpfs",    "mode=1777",   MS_NOSUID|MS_NODEV },
        { "tmpfs",    "/run",     "tmpfs",    "mode=0755",   MS_NOSUID|MS_NODEV },
    };
    for (size_t i = 0; i < sizeof(mounts)/sizeof(mounts[0]); i++) {
        if (mount(mounts[i].src, mounts[i].tgt, mounts[i].fs,
                  mounts[i].fl, mounts[i].opt) < 0 && errno != EBUSY) {
            char msg[128];
            snprintf(msg, sizeof(msg), "minit: mount %s: %s\n",
                     mounts[i].tgt, strerror(errno));
            (void)write(STDERR_FILENO, msg, strlen(msg));
        }
    }
}
static void open_console(void) {
    const char *devs[] = { "/dev/console", "/dev/tty1", "/dev/tty", NULL };
    int fd = -1;
    for (int i = 0; devs[i]; i++) {
        fd = open(devs[i], O_RDWR | O_NOCTTY);
        if (fd >= 0) break;
    }
    if (fd < 0) return;
    if (!isatty(STDIN_FILENO))  dup2(fd, STDIN_FILENO);
    if (!isatty(STDOUT_FILENO)) dup2(fd, STDOUT_FILENO);
    if (!isatty(STDERR_FILENO)) dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO) close(fd);
    setsid();
}
static void sanitise_env(void) {
    const char *keep[] = { "TERM", NULL };
    char path[]  = "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    char *env[3] = { path, NULL, NULL };
    size_t n = 1;
    const char *term = getenv("TERM");
    char termbuf[64];
    if (term) {
        snprintf(termbuf, sizeof(termbuf), "TERM=%s", term);
        env[n++] = termbuf;
    }
    (void)keep;
    for (size_t i = 0; env[i]; i++) putenv(env[i]);
    unsetenv("HOME");
    unsetenv("USER");
    unsetenv("LOGNAME");
    unsetenv("MAIL");
}
static void write_utmp_boot(void) {
    struct utmp ut;
    memset(&ut, 0, sizeof(ut));
    ut.ut_type = BOOT_TIME;
    time((time_t *)&ut.ut_tv.tv_sec);
    setutent();
    pututline(&ut);
    endutent();
}
static int run_stage(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) return 0;
    if (!(st.st_mode & S_IXUSR)) return 0;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        char *argv[] = { (char *)path, NULL };
        setsid();
        execv(path, argv);
        char msg[128];
        snprintf(msg, sizeof(msg), "minit: exec %s: %s\n", path, strerror(errno));
        (void)write(STDERR_FILENO, msg, strlen(msg));
        _exit(111);
    }
    int status = 0;
    for (;;) {
        pid_t w = waitpid(pid, &status, 0);
        if (w == pid) break;
        if (w < 0 && errno != EINTR) break;
        reap();
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (getpid() != 1) {
        fputs("minit: must run as PID 1\n", stderr);
        return 1;
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sa.sa_handler = SIG_DFL;
    for (int i = 1; i < NSIG; i++) sigaction(i, &sa, NULL);
    sa.sa_flags   = SA_RESTART;
    sa.sa_handler = on_sigint;  sigaction(SIGINT,  &sa, NULL);
    sa.sa_handler = on_sigterm; sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = on_sigpwr;  sigaction(SIGPWR,  &sa, NULL);
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    sa.sa_handler = on_sigchld; sigaction(SIGCHLD, &sa, NULL);
    sa.sa_handler = SIG_IGN;    sigaction(SIGHUP,  &sa, NULL);
    sa.sa_handler = SIG_IGN;    sigaction(SIGPIPE, &sa, NULL);
    sa.sa_handler = SIG_IGN;    sigaction(SIGTTOU, &sa, NULL);
    sa.sa_handler = SIG_IGN;    sigaction(SIGTTIN, &sa, NULL);
    umask(022);
    mount_essential();
    open_console();
    sanitise_env();
    write_utmp_boot();
    int sigs_to_block[] = { SIGCHLD, SIGINT, SIGTERM, SIGPWR };
    sigmask_set(SIG_BLOCK, sigs_to_block, 4);
    run_stage("/etc/runit/1");
    sigmask_set(SIG_UNBLOCK, sigs_to_block, 4);
    g_runit_pid = fork();
    if (g_runit_pid < 0) {
        (void)write(STDERR_FILENO, "minit: fork stage 2 failed\n", 27);
        do_halt(RB_HALT_SYSTEM);
    }
    if (g_runit_pid == 0) {
        setsid();
        char *argv2[] = { "/etc/runit/2", NULL };
        execv("/etc/runit/2", argv2);
        char msg[128];
        snprintf(msg, sizeof(msg), "minit: exec /etc/runit/2: %s\n", strerror(errno));
        (void)write(STDERR_FILENO, msg, strlen(msg));
        _exit(111);
    }
    int stage2_status = 0;
    for (;;) {
        pid_t w = waitpid(g_runit_pid, &stage2_status, WNOHANG);
        if (w == g_runit_pid) break;
        if (w < 0 && errno != EINTR) break;
        if (g_sigchld) { g_sigchld = 0; reap(); }
        forward_to_runit();
        struct timespec ts = { 0, 100000000L };
        nanosleep(&ts, NULL);
    }
    g_runit_pid = -1;
    reap();
    int stage3_ret = run_stage("/etc/runit/3");
    if (stage3_ret == 1)
        do_halt(RB_AUTOBOOT);
    if (stage3_ret == 2)
        do_halt(RB_POWER_OFF);
    if (WIFEXITED(stage2_status) && WEXITSTATUS(stage2_status) == 2)
        do_halt(RB_POWER_OFF);
    do_halt(RB_HALT_SYSTEM);
    return 0;
}
