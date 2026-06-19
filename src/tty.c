/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <barelib.h>
static bool flag_help = false;
static bool flag_silent = false;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "tty", "tty [OPTION]...", "Print the file name of the terminal connected to standard input.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 's', "silent", "print nothing, only return an exit status", BARE_OPT_BOOL, {.bval=&flag_silent}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }
    if (!isatty(STDIN_FILENO)) {
        if (!flag_silent)
            puts("not a tty");
        bare_cli_free(&cli);
        return 1;
    }
    if (!flag_silent) {
        char *name = ttyname(STDIN_FILENO);
        puts(name ? name : "/dev/tty");
    }
    bare_cli_free(&cli);
    return 0;
}
