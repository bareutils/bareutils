/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <barelib.h>
static bool flag_help = false;
static bool flag_zero = false;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "dirname", "dirname [OPTION] NAME...", "Print NAME with its last non-slash component and trailing slashes removed.\nIf NAME contains no '/', output '.' (meaning the current directory).");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'z', "zero", "end each output line with NUL, not newline", BARE_OPT_BOOL, {.bval=&flag_zero}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }
    if (cli.npositional < 1 || !cli.positional[0])
        bare_die("dirname", "missing operand");
    for (size_t i = 0; i < cli.npositional; i++) {
        const char *arg = cli.positional[i];
        if (!arg)
            continue;
        char *buf = bare_strdup(arg);
        size_t len = strlen(buf);
        while (len > 1 && buf[len - 1] == '/')
            buf[--len] = '\0';
        if (len == 1 && buf[0] == '/') {
            printf("/%c", flag_zero ? '\0' : '\n');
            free(buf);
            continue;
        }
        char *result = bare_path_dirname(buf);
        if (result[0] == '.' && result[1] == '\0' && buf[0] == '/') {
            free(result);
            result = bare_strdup("/");
        }
        printf("%s%c", result, flag_zero ? '\0' : '\n');
        free(result);
        free(buf);
    }
    bare_cli_free(&cli);
    return 0;
}
