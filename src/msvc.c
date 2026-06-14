/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define STATUS_SIZE 20
typedef struct {
    uint64_t tai;
    uint32_t nano;
    pid_t    pid;
    bool     paused;
    uint8_t  want;
    bool     term_sig;
    uint8_t  state;
} svc_status_t;
static bool flag_help = false;
static const struct { const char *name; char ctl; } commands[] = {
    { "up",      'u' },
    { "down",    'd' },
    { "once",    'o' },
    { "pause",   'p' },
    { "cont",    'c' },
    { "hup",     'h' },
    { "alarm",   'a' },
    { "int",     'i' },
    { "quit",    'q' },
    { "usr1",    '1' },
    { "usr2",    '2' },
    { "term",    't' },
    { "kill",    'k' },
    { "exit",    'x' },
    { "restart", 0   },
    { NULL, 0 },
};
static int read_status(const char *svcdir, svc_status_t *out) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/supervise/status", svcdir);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    uint8_t buf[STATUS_SIZE];
    ssize_t n = read(fd, buf, STATUS_SIZE);
    close(fd);
    if (n < STATUS_SIZE) return -1;
    out->tai  = ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48)
              | ((uint64_t)buf[2] << 40) | ((uint64_t)buf[3] << 32)
              | ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16)
              | ((uint64_t)buf[6] <<  8) |  (uint64_t)buf[7];
    out->nano = ((uint32_t)buf[8]  << 24) | ((uint32_t)buf[9]  << 16)
              | ((uint32_t)buf[10] <<  8) |  (uint32_t)buf[11];
    out->pid  = ((uint32_t)buf[12] << 24) | ((uint32_t)buf[13] << 16)
              | ((uint32_t)buf[14] <<  8) |  (uint32_t)buf[15];
    out->paused  = buf[16];
    out->want    = buf[17];
    out->term_sig = buf[18];
    out->state   = buf[19];
    return 0;
}
static void print_status(const char *svcdir) {
    const char *name = svcdir;
    const char *sl = strrchr(svcdir, '/');
    if (sl) name = sl + 1;
    svc_status_t st;
    if (read_status(svcdir, &st) < 0) {
        printf("%-24s  supervise not running\n", name);
        return;
    }
    time_t now   = time(NULL);
    time_t since = (time_t)(st.tai - 4611686018427387914ULL);
    long   secs  = (long)(now - since);
    if (secs < 0) secs = 0;
    const char *state;
    switch (st.state) {
    case 1:  state = "run";    break;
    case 2:  state = "finish"; break;
    default: state = "down";   break;
    }
    printf("%-24s  %s", name, state);
    if (st.state != 0 && st.pid > 0)
        printf(" (pid %d)", (int)st.pid);
    printf(", %ld seconds", secs);
    if (st.paused)   printf(", paused");
    if (st.want == 'u' && st.state == 0) printf(", want up");
    if (st.want == 'd' && st.state != 0) printf(", want down");
    printf("\n");
}
static int check_ok(const char *svcdir) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/supervise/ok", svcdir);
    int fd = open(path, O_WRONLY | O_NONBLOCK);
    if (fd < 0) return -1;
    close(fd);
    return 0;
}
static int send_ctl(const char *svcdir, char c) {
    char path[4096];
    snprintf(path, sizeof(path), "%s/supervise/control", svcdir);
    int fd = open(path, O_WRONLY | O_NONBLOCK);
    if (fd < 0) return -1;
    char buf[1] = { c };
    ssize_t n = write(fd, buf, 1);
    close(fd);
    return n == 1 ? 0 : -1;
}
static int do_command(const char *cmd, const char *svcdir) {
    const char *name = svcdir;
    const char *sl = strrchr(svcdir, '/');
    if (sl) name = sl + 1;
    if (strcmp(cmd, "status") == 0) {
        print_status(svcdir);
        return 0;
    }
    if (strcmp(cmd, "check") == 0) {
        if (check_ok(svcdir) < 0) return 2;
        svc_status_t st;
        if (read_status(svcdir, &st) < 0) return 4;
        return (st.state == 1 && st.want == 'u') ? 0 : 1;
    }
    if (check_ok(svcdir) < 0) {
        fprintf(stderr, "msvc: %s: supervisor not running\n", name);
        return 2;
    }
    if (strcmp(cmd, "restart") == 0) {
        if (send_ctl(svcdir, 'd') < 0) goto fail;
        struct timespec ts = { 0, 100000000L };
        for (int i = 0; i < 50; i++) {
            nanosleep(&ts, NULL);
            svc_status_t st;
            if (read_status(svcdir, &st) < 0) continue;
            if (st.state == 0) break;
        }
        if (send_ctl(svcdir, 'u') < 0) goto fail;
        return 0;
    }
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(commands[i].name, cmd) == 0 && commands[i].ctl != 0) {
            if (send_ctl(svcdir, commands[i].ctl) < 0) goto fail;
            return 0;
        }
    }
    fprintf(stderr, "msvc: unknown command: %s\n", cmd);
    return 4;
fail:
    fprintf(stderr, "msvc: %s: %s: %s\n", cmd, name, strerror(errno));
    return 1;
}
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "msvc", "msvc [options] <command> <svcdir>...",
                  "Control and inspect runit-style services.\n"
                  "Commands: up down once pause cont hup alarm int quit usr1 usr2\n"
                  "          term kill restart exit status check");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }
    if (cli.npositional < 2) bare_die("msvc", "usage: msvc <command> <svcdir>...");
    const char *cmd = cli.positional[0];
    int rc = 0;
    for (size_t i = 1; i < cli.npositional; i++) {
        int r = do_command(cmd, cli.positional[i]);
        if (r != 0) rc = r;
    }
    bare_cli_free(&cli);
    return rc;
}
