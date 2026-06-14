/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <unistd.h>
#include <pwd.h>
static bool flag_help = false;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "whoami", "whoami", "Print the effective user name.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }

    struct passwd *pw = getpwuid(geteuid());
    if (!pw) bare_die("whoami", "cannot find name for effective uid");
    printf("%s\n", pw->pw_name);
    bare_cli_free(&cli);
    return 0;
}
