/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/syscall.h>
#include <barelib.h>
static bool flag_help = false;
static bool flag_fs = false;
static bool flag_data = false;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "sync", "sync [OPTION]... [FILE]...", "Synchronize cached writes to persistent storage.\nIf one or more files are specified, sync only their contents.\nOtherwise, sync the entire system.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'f', "file-system", "sync the file systems that contain the files", BARE_OPT_BOOL, {.bval=&flag_fs}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'd', "data", "sync only file data, no unneeded metadata", BARE_OPT_BOOL, {.bval=&flag_data}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }
    if (cli.npositional == 0) {
        sync();
        bare_cli_free(&cli);
        return 0;
    }
    for (size_t i = 0; i < cli.npositional; i++) {
        const char *path = cli.positional[i];
        if (!path)
            continue;
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "sync: %s: %s\n", path, strerror(errno));
            continue;
        }
        if (flag_fs) {
#if defined(__linux__) && defined(SYS_syncfs)
            if (syscall(SYS_syncfs, fd) < 0)
                fprintf(stderr, "sync: %s: %s\n", path, strerror(errno));
#else
            fprintf(stderr, "sync: -f not supported on this system\n");
            close(fd);
            bare_cli_free(&cli);
            return 1;
#endif
        } else if (flag_data) {
            if (fdatasync(fd) < 0)
                fprintf(stderr, "sync: %s: %s\n", path, strerror(errno));
        } else {
            if (fsync(fd) < 0)
                fprintf(stderr, "sync: %s: %s\n", path, strerror(errno));
        }
        close(fd);
    }
    bare_cli_free(&cli);
    return 0;
}
