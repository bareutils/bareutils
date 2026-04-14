/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static bool flag_all       = false;
static bool flag_long      = false;
static bool flag_human     = false;
static bool flag_sort_time = false;
static bool flag_sort_size = false;
static bool flag_reverse   = false;
static bool flag_color     = false;
static bool flag_one       = false;
static bool flag_classify  = false;
static bool flag_almost_all = false;
static bool flag_help      = false;

static char get_classifier(const bare_entry_t *e) {
    if (e->type == BARE_FT_DIR) return '/';
    if (e->type == BARE_FT_LINK) return '@';
    if (e->type == BARE_FT_FIFO) return '|';
    if (e->type == BARE_FT_SOCK) return '=';
    if (e->type == BARE_FT_REG && (e->mode & (S_IXUSR | S_IXGRP | S_IXOTH))) return '*';
    return '\0';
}
static void apply_color(FILE *f, const bare_entry_t *e) {
    if (!bare_isatty(f)) return;
    if (e->type == BARE_FT_DIR) {
        bare_attr_set(f, BARE_ATTR_BOLD);
        bare_color_set(f, BARE_COLOR_BBLUE, BARE_COLOR_RESET);
    } else if (e->type == BARE_FT_LINK) {
        bare_color_set(f, BARE_COLOR_BCYAN, BARE_COLOR_RESET);
    } else if (e->type == BARE_FT_FIFO || e->type == BARE_FT_SOCK) {
        bare_color_set(f, BARE_COLOR_BYELLOW, BARE_COLOR_RESET);
    } else if (e->mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
        bare_attr_set(f, BARE_ATTR_BOLD);
        bare_color_set(f, BARE_COLOR_BGREEN, BARE_COLOR_RESET);
    }
}

static void print_long(const bare_entry_t *e, bool human) {
    char *mode  = bare_fmt_mode(e->mode);
    char *tstr  = bare_fmt_time(e->mtime, NULL);
    char *owner = bare_fmt_owner(e->uid, e->gid);

    if (human) {
        char *sz = bare_fmt_size((unsigned long long)e->size);
        printf("%s %2lu %s %6s ", mode, (unsigned long)e->nlink, owner, sz);
        bare_free(sz);
    } else {
        printf("%s %2lu %s %8lld ", mode, (unsigned long)e->nlink, owner, (long long)e->size);
    }

    printf("%s ", tstr);

    if (flag_color) apply_color(stdout, e);
    printf("%s", e->name);
    if (flag_color) bare_color_reset(stdout);

    if (flag_classify) {
        char c = get_classifier(e);
        if (c) putchar(c);
    }

    if (e->type == BARE_FT_LINK) {
        char lbuf[4096];
        ssize_t ln = readlink(e->path, lbuf, sizeof(lbuf) - 1);
        if (ln > 0) {
            lbuf[ln] = '\0';
            printf(" -> %s", lbuf);
        }
    }

    putchar('\n');
    bare_free(mode);
    bare_free(tstr);
    bare_free(owner);
}

static void ls_dir(const char *path) {
    bare_dir_t dir;
    bare_err_t err = bare_dir_read(path, &dir);
    if (err != BARE_OK)
        bare_die_err("ls", err);

    size_t j = 0;
    for (size_t i = 0; i < dir.count; i++) {
        bool show = false;
        if (flag_all) {
            show = true;
        } else if (flag_almost_all) {
            if (strcmp(dir.items[i].name, ".") != 0 && strcmp(dir.items[i].name, "..") != 0)
                show = true;
        } else {
            if (!dir.items[i].is_hidden)
                show = true;
        }

        if (show) {
            dir.items[j++] = dir.items[i];
        } else {
            bare_entry_free(&dir.items[i]);
        }
    }
    dir.count = j;

    if (flag_sort_time)       bare_dir_sort_mtime(&dir);
    else if (flag_sort_size)  bare_dir_sort_size(&dir);
    else                      bare_dir_sort_name(&dir);

    if (flag_reverse) {
        for (size_t i = 0, j = dir.count - 1; i < j; i++, j--) {
            bare_entry_t tmp = dir.items[i];
            dir.items[i]     = dir.items[j];
            dir.items[j]     = tmp;
        }
    }

    if (flag_long) {
        unsigned long long total = 0;
        for (size_t i = 0; i < dir.count; i++)
            total += (unsigned long long)dir.items[i].size;
        if (flag_human) {
            char *ts = bare_fmt_size(total);
            printf("total %s\n", ts);
            bare_free(ts);
        } else {
            printf("total %llu\n", total / 2);
        }
        for (size_t i = 0; i < dir.count; i++)
            print_long(&dir.items[i], flag_human);
    } else if (flag_one) {
        for (size_t i = 0; i < dir.count; i++) {
            if (flag_color) apply_color(stdout, &dir.items[i]);
            printf("%s", dir.items[i].name);
            if (flag_classify) {
                char c = get_classifier(&dir.items[i]);
                if (c) putchar(c);
            }
            if (flag_color) bare_color_reset(stdout);
            putchar('\n');
        }
    } else {
        int tw = bare_term_width();
        const char **names = bare_malloc(dir.count * sizeof(char *));
        bool *allocated = bare_malloc(dir.count * sizeof(bool));
        for (size_t i = 0; i < dir.count; i++) {
            char c = flag_classify ? get_classifier(&dir.items[i]) : '\0';
            if (c) {
                char* name = bare_malloc(strlen(dir.items[i].name) + 2);
                sprintf(name, "%s%c", dir.items[i].name, c);
                names[i] = name;
                allocated[i] = true;
            } else {
                names[i] = dir.items[i].name;
                allocated[i] = false;
            }
        }
        bare_print_columns(stdout, names, dir.count, tw);
        for (size_t i = 0; i < dir.count; i++) {
            if (allocated[i]) bare_free((void *)names[i]);
        }
        bare_free((void *)names);
        bare_free(allocated);
    }

    bare_dir_free(&dir);
}

int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "ls", "ls [options] [file...]",
                  "List directory contents.");

    bare_cli_add_opt(&cli, (bare_opt_t){ 'a', "all",        "show hidden entries",         BARE_OPT_BOOL,   {.bval=&flag_all},        false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'A', "almost-all", "do not list implicit . and ..", BARE_OPT_BOOL, {.bval=&flag_almost_all}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'l', "long",      "long listing format",         BARE_OPT_BOOL,   {.bval=&flag_long},      false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'h', "human",     "human-readable sizes",        BARE_OPT_BOOL,   {.bval=&flag_human},     false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 't', "sort-time", "sort by modification time",   BARE_OPT_BOOL,   {.bval=&flag_sort_time}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'S', "sort-size", "sort by file size",           BARE_OPT_BOOL,   {.bval=&flag_sort_size}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'r', "reverse",   "reverse sort order",          BARE_OPT_BOOL,   {.bval=&flag_reverse},   false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'c', "color",     "colorize output",             BARE_OPT_BOOL,   {.bval=&flag_color},     false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'F', "classify",  "append indicator (one of */=>@|) to entries", BARE_OPT_BOOL, {.bval=&flag_classify}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ '1', NULL,        "one entry per line",          BARE_OPT_BOOL,   {.bval=&flag_one},       false });
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help",      "show this help and exit",     BARE_OPT_BOOL,   {.bval=&flag_help},      false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }

    if (!bare_isatty(stdout)) flag_color = false;
    if (cli.npositional == 0) {
        ls_dir(".");
    } else {
        for (size_t i = 0; i < cli.npositional; i++) {
            if (cli.npositional > 1)
                printf("%s:\n", cli.positional[i]);
            if (bare_path_is_dir(cli.positional[i]))
                ls_dir(cli.positional[i]);
            else {
                bare_entry_t entry = {0};
                if (bare_entry_stat(cli.positional[i], &entry) != BARE_OK) {
                    char* filename = bare_malloc(strlen(cli.positional[i] + 2));
                    sprintf(filename, "%s':", cli.positional[i]);
                    bare_die_errno("ls: cannot access '", filename);
                }
                if (flag_long)
                    print_long(&entry, flag_human);
                else {
                    if (flag_color) apply_color(stdout, &entry);
                    printf("%s", entry.name);
                    if (flag_classify) {
                        char c = get_classifier(&entry);
                        if (c) putchar(c);
                    }
                    if (flag_color) bare_color_reset(stdout);
                    putchar('\n');
                }
                bare_entry_free(&entry);
            }
            if (cli.npositional > 1 && i + 1 < cli.npositional)
                putchar('\n');
        }
    }

    bare_cli_free(&cli);
    return 0;
}
