/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/file.h>
static bool flag_help = false;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "nologin", "nologin", "Politely refuse a login.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }

    FILE* fd = fopen("/etc/nologin", "r");
    if (fd) {
        char buf[256];
        while (fgets(buf, sizeof(buf), fd)) {
            fputs(buf, stderr);
        }
        fclose(fd);
    } else {
        fputs("This account is currently not available.\n", stderr);
    }
    bare_cli_free(&cli);
    return 1;
}
