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
    bare_cli_init(&cli, "usleep", "usleep MICROSECONDS", "Sleep for MICROSECONDS microseconds.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }

    if (cli.npositional < 1) bare_die("usleep", "missing operand");
    char *end;
    unsigned long usec = strtoul(cli.positional[0], &end, 10);
    if (end == cli.positional[0] || *end != '\0')
        bare_die("usleep", "invalid number");

    struct timespec ts;
    ts.tv_sec = usec / 1000000;
    ts.tv_nsec = (usec % 1000000) * 1000;
    struct timespec rem;
    while (nanosleep(&ts, &rem) == -1 && errno == EINTR)
        ts = rem;

    bare_cli_free(&cli);
    return 0;
}
