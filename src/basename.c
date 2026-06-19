/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <barelib.h>
static bool flag_help = false;
static bool flag_multiple = false;
static bool flag_zero = false;
static char *flag_suffix = NULL;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "basename", "basename NAME [SUFFIX]\n  or:  basename OPTION... NAME...", "Print NAME with any leading directory components removed.\nIf specified, also remove a trailing SUFFIX.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'a', "multiple", "support multiple arguments and treat each as a NAME", BARE_OPT_BOOL, {.bval=&flag_multiple}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 's', "suffix", "remove a trailing SUFFIX; implies -a", BARE_OPT_STRING, {.sval=&flag_suffix}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'z', "zero", "end each output line with NUL, not newline", BARE_OPT_BOOL, {.bval=&flag_zero}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }
    if (flag_suffix)
        flag_multiple = true;
    const char *suffix = flag_suffix;
    if (!flag_multiple) {
        if (cli.npositional < 1 || !cli.positional[0])
            bare_die("basename", "missing operand");
    }
    for (size_t i = 0; i < (flag_multiple ? cli.npositional : 1); i++) {
        const char *arg = flag_multiple ? cli.positional[i] : cli.positional[0];
        if (!arg) continue;
        const char *suf = suffix;
        if (!flag_multiple && cli.npositional >= 2 && cli.positional[1])
            suf = cli.positional[1];
        char *buf = bare_strdup(arg);
        size_t len = strlen(buf);
        while (len > 1 && buf[len - 1] == '/')
            buf[--len] = '\0';
        const char *base = buf;
        char *last = strrchr(buf, '/');
        if (last) {
            base = last + 1;
            if (*base == '\0' && last == buf)
                base = "/";
        }
        size_t blen = strlen(base);
        size_t slen = suf ? strlen(suf) : 0;
        if (slen > 0 && blen >= slen && strcmp(base + blen - slen, suf) == 0)
            blen -= slen;
        printf("%.*s%c", (int)blen, base, flag_zero ? '\0' : '\n');
        free(buf);
    }
    bare_cli_free(&cli);
    return 0;
}
