/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <string.h>
#include <unistd.h>
#include <barextras.h>
#include <barelib.h>
#include <limits.h>
static bool flag_help = false;
static bool flag_esc_e = false;
static bool flag_esc_d = true; //neo: unused
static bool flag_nonewline = false;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "echo", "echo [OPTION(s)]... [STRING]...", "Echo the STRING(s) to standard output.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'n', NULL, "  do not print trailing newline", BARE_OPT_BOOL, {.bval=&flag_nonewline}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'e', NULL, "  enable interpretation of backslash escapes", BARE_OPT_BOOL, {.bval=&flag_esc_e}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'E', NULL, "  disable interpretation of backslash escapes (default)", BARE_OPT_BOOL, {.bval=&flag_esc_d}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }
    unsigned int i = 0;
    do {
        if (cli.positional[i] == NULL) { i++; continue; }
        if (flag_esc_e) {
            char *s = esc(cli.positional[i]);
            printf("%s", s);
            free(s);
            i++;
            continue;
        }
        printf("%s ", cli.positional[i]);
        i++;
    } while (i < cli.npositional);
    if (!flag_nonewline) {printf("\n");}
    bare_cli_free(&cli);
    return 0;
}
