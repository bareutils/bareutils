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
    bare_cli_init(&cli, "sleep", "sleep NUMBER[SUFFIX]...", "Pause for NUMBER seconds. SUFFIX may be 's' for seconds (default),\n'm' for minutes, 'h' for hours or 'd' for days.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }

    double total = 0.0;
    for (size_t i = 0; i < cli.npositional; i++) {
        char *end;
        double d = strtod(cli.positional[i], &end);
        if (end == cli.positional[i]) bare_die("sleep", "invalid time interval");
        switch (*end) {
            case 's': break;
            case 'm': d *= 60.0; break;
            case 'h': d *= 3600.0; break;
            case 'd': d *= 86400.0; break;
            case '\0': break;
            default: bare_die("sleep", "invalid time suffix");
        }
        total += d;
    }

    if (total > 0.0) {
        struct timespec ts;
        ts.tv_sec = (time_t)total;
        ts.tv_nsec = (long)((total - (time_t)total) * 1.0e9);
        struct timespec rem;
        while (nanosleep(&ts, &rem) == -1 && errno == EINTR)
            ts = rem;
    }

    bare_cli_free(&cli);
    return 0;
}
