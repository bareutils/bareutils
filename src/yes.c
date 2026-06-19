/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <barelib.h>
static bool flag_help = false;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "yes", "yes [STRING]...", "Repeatedly output a line with all specified STRING(s), or 'y'.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }
    if (cli.npositional == 0) {
        for (;;)
            printf("y\n");
    }
    for (;;) {
        for (size_t i = 0; i < cli.npositional; i++) {
            if (i > 0)
                putchar(' ');
            printf("%s", cli.positional[i]);
        }
        putchar('\n');
    }
    bare_cli_free(&cli);
    return 0;
}
