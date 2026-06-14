/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
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

    int fd = open("/etc/nologin.txt", O_RDONLY);
    if (fd == -1) {
        ssize_t ignored = write(STDERR_FILENO, "This account is currently not available.\n", 41);
        (void)ignored;
        bare_cli_free(&cli);
        return 1;
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    void *buffer = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buffer == MAP_FAILED) {
        close(fd);
        ssize_t ignored = write(STDERR_FILENO, "This account is currently not available.\n", 41);
        (void)ignored;
        bare_cli_free(&cli);
        return 1;
    }

    ssize_t ignored = write(STDOUT_FILENO, buffer, file_size);
    (void)ignored;
    munmap(buffer, file_size);
    close(fd);
    bare_cli_free(&cli);
    return 1;
}
