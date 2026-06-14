/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <time.h>
#include <errno.h>
static bool flag_help = false;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "nanosleep", "nanosleep TIME", "Sleep with nanosecond precision. TIME may be a floating-point number (seconds)\nor two integers: SECONDS NANOSECONDS.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }

    if (cli.npositional < 1) bare_die("nanosleep", "missing operand");
    struct timespec ts = { 0, 0 };
    if (cli.npositional == 1) {
        char *end;
        double d = strtod(cli.positional[0], &end);
        if (end == cli.positional[0] || *end != '\0')
            bare_die("nanosleep", "invalid time");
        if (d < 0) bare_die("nanosleep", "negative time");
        ts.tv_sec = (time_t)d;
        ts.tv_nsec = (long)((d - (time_t)d) * 1.0e9);
    } else {
        char *end;
        long sec = strtol(cli.positional[0], &end, 10);
        if (end == cli.positional[0] || *end != '\0')
            bare_die("nanosleep", "invalid seconds");
        long nsec = strtol(cli.positional[1], &end, 10);
        if (end == cli.positional[1] || *end != '\0')
            bare_die("nanosleep", "invalid nanoseconds");
        if (sec < 0 || nsec < 0) bare_die("nanosleep", "negative time");
        ts.tv_sec = sec + nsec / 1000000000;
        ts.tv_nsec = nsec % 1000000000;
    }

    struct timespec rem;
    while (nanosleep(&ts, &rem) == -1 && errno == EINTR)
        ts = rem;

    bare_cli_free(&cli);
    return 0;
}
