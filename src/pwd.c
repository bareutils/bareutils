/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <unistd.h>
#include <limits.h>
static bool flag_help     = false;
static bool flag_logical  = false;
static bool flag_physical = false;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "pwd", "pwd", "Print the full filename of the current working directory.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help",     "show this help and exit",         BARE_OPT_BOOL, {.bval=&flag_help},     false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'L', "logical",  "use PWD from environment, even if it contains symlinks", BARE_OPT_BOOL, {.bval=&flag_logical},  false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'P', "physical", "resolve all symlinks",            BARE_OPT_BOOL, {.bval=&flag_physical}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }

    char buffer[PATH_MAX];
    if (flag_physical) {
        if (getcwd(buffer, sizeof(buffer)) != NULL) {printf("%s\n", buffer);} else {bare_cli_free(&cli); bare_die("pwd", "getcwd() failed"); return 1;}
    } else {
        const char *pwd = getenv("PWD");
        if (pwd != NULL) {
            printf("%s\n", pwd);
        } else {
            if (getcwd(buffer, sizeof(buffer)) != NULL) {printf("%s\n", buffer);} else {bare_cli_free(&cli); bare_die("pwd", "getcwd() failed"); return 1;}
        }
    }
    bare_cli_free(&cli);
    return 0;
}
