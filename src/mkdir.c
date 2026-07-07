/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
static bool flag_help = false;
static bool flag_parents = false;
static bool flag_verbose = false;
static char *flag_mode = NULL;
static mode_t parse_mode(const char *s) {
    char *end;
    long m = strtol(s, &end, 8);
    if (*end == '\0' && m >= 0 && m <= 07777) return (mode_t)m;
    bare_die("mkdir", "invalid mode");
    return 0;
}

static void create(const char *path, mode_t mode, bool set_mode) {
    if (set_mode) {
        mode_t old = umask(0);
        int ret = mkdir(path, mode);
        umask(old);
        if (ret == -1) {
            if (errno == EEXIST && flag_parents) return;
            bare_die("mkdir", "cannot create directory");
        }
    } else {
        if (mkdir(path, mode) == -1) {
            if (errno == EEXIST && flag_parents) return;
            bare_die("mkdir", "cannot create directory");
        }
    }
    if (flag_verbose)
        printf("mkdir: created directory '%s'\n", path);
}

int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "mkdir", "mkdir [OPTION]... DIRECTORY...", "Create the DIRECTORY(ies), if they do not already exist.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "display this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'm', "mode", "set file mode (as in chmod), not a=rwx - umask", BARE_OPT_STRING, {.sval=&flag_mode}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'p', "parents", "no error if existing, make parent directories as needed, with their file modes unaffected by any -m option", BARE_OPT_BOOL, {.bval=&flag_parents}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'v', "verbose", "print a message for each created directory", BARE_OPT_BOOL, {.bval=&flag_verbose}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }

    if (cli.npositional == 0) bare_die("mkdir", "missing operand");
    mode_t mode = flag_mode ? parse_mode(flag_mode) : 0777;
    bool need_chmod = flag_mode != NULL;
    for (size_t i = 0; i < cli.npositional; i++) {
        const char *arg = cli.positional[i];
        if (flag_parents) {
            char *path = bare_strdup(arg);
            size_t len = strlen(path);
            if (len > 1 && path[len - 1] == '/')
                path[len - 1] = '\0';
            char *p = path;
            while (*p == '/') p++;
            while (*p) {
                char *slash = strchr(p, '/');
                if (slash) *slash = '\0';
                if (bare_path_exists(path)) {
                    if (!bare_path_is_dir(path))
                        bare_die("mkdir", "path exists but is not a directory");
                } else {
                    if (mkdir(path, 0777) == -1)
                        bare_die("mkdir", "cannot create directory");
                    if (flag_verbose)
                        printf("mkdir: created directory '%s'\n", path);
                }
                if (!slash) {
                    if (need_chmod && chmod(path, mode) == -1)
                        bare_die("mkdir", "cannot set permissions");
                    break;
                }
                *slash = '/';
                p = slash + 1;
                while (*p == '/') p++;
            }
            free(path);
        } else {
            create(arg, mode, need_chmod);
        }
    }

    bare_cli_free(&cli);
    return 0;
}
