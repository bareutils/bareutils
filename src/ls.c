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
static char *opt_block_size = NULL;
static bool flag_ignore_backups = false;
static bool flag_zero      = false;
static bool flag_sort_extension = false;
static bool flag_horizontal = false;
static bool flag_recursive  = false;
static bool flag_directory  = false;
static long long parse_block_size(const char *s) {
    if (!s) return 1;
    char *end;
    long long val = strtoll(s, &end, 10);
    if (end == s) val = 1;
    if (*end == '\0') return val;
    long long mult = 1;
    if (strcmp(end, "K") == 0 || strcmp(end, "KiB") == 0) mult = 1024LL;
    else if (strcmp(end, "M") == 0 || strcmp(end, "MiB") == 0) mult = 1024LL * 1024;
    else if (strcmp(end, "G") == 0 || strcmp(end, "GiB") == 0) mult = 1024LL * 1024 * 1024;
    else if (strcmp(end, "T") == 0 || strcmp(end, "TiB") == 0) mult = 1024LL * 1024 * 1024 * 1024;
    else if (strcmp(end, "KB") == 0) mult = 1000LL;
    else if (strcmp(end, "MB") == 0) mult = 1000LL * 1000;
    else if (strcmp(end, "GB") == 0) mult = 1000LL * 1000 * 1000;
    else if (strcmp(end, "TB") == 0) mult = 1000LL * 1000 * 1000 * 1000;
    return val * mult;
}

static int compare_extension(const void *a, const void *b) {
    const bare_entry_t *ea = a;
    const bare_entry_t *eb = b;
    const char *exta = strrchr(ea->name, '.');
    const char *extb = strrchr(eb->name, '.');
    if (!exta) exta = "";
    if (!extb) extb = "";
    int res = strcmp(exta, extb);
    if (res == 0) return strcmp(ea->name, eb->name);
    return res;
}

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
    long long bs = parse_block_size(opt_block_size);
    if (human) {
        char *sz = bare_fmt_size((unsigned long long)e->size);
        printf("%s %2lu %s %6s ", mode, (unsigned long)e->nlink, owner, sz);
        bare_free(sz);
    } else if (bs > 1) {
        printf("%s %2lu %s %8lld ", mode, (unsigned long)e->nlink, owner, (long long)((e->size + bs - 1) / bs));
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
    if (err != BARE_OK) bare_die_err("ls", err);
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

        if (show && flag_ignore_backups) {
            size_t len = strlen(dir.items[i].name);
            if (len > 0 && dir.items[i].name[len-1] == '~')
                show = false;
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
    else if (flag_sort_extension) qsort(dir.items, dir.count, sizeof(bare_entry_t), compare_extension);
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
        long long bs = parse_block_size(opt_block_size);
        if (flag_human) {
            char *ts = bare_fmt_size(total);
            printf("total %s\n", ts);
            bare_free(ts);
        } else if (bs > 1) {
            printf("total %lld\n", (long long)((total + bs - 1) / bs));
        } else {
            printf("total %llu\n", total / 1024);
        }
        for (size_t i = 0; i < dir.count; i++)
            print_long(&dir.items[i], flag_human);
    } else if (flag_one || flag_zero) {
        for (size_t i = 0; i < dir.count; i++) {
            if (flag_color) apply_color(stdout, &dir.items[i]);
            printf("%s", dir.items[i].name);
            if (flag_classify) {
                char c = get_classifier(&dir.items[i]);
                if (c) putchar(c);
            }
            if (flag_color) bare_color_reset(stdout);
            putchar(flag_zero ? '\0' : '\n');
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
        if (flag_horizontal) {
            int col_width = 0;
            for (size_t i = 0; i < dir.count; i++) {
                int len = strlen(names[i]);
                if (len > col_width) col_width = len;
            }
            col_width += 2;
            int cols = tw / col_width;
            if (cols < 1) cols = 1;
            for (size_t i = 0; i < dir.count; i++) {
                printf("%-*s", col_width, names[i]);
                if ((i + 1) % cols == 0 || i + 1 == dir.count) putchar('\n');
            }
        } else {
            bare_print_columns(stdout, names, dir.count, tw);
        }
        for (size_t i = 0; i < dir.count; i++) {
            if (allocated[i]) bare_free((void *)names[i]);
        }
        bare_free((void *)names);
        bare_free(allocated);
    }

    if (flag_recursive) {
        for (size_t i = 0; i < dir.count; i++) {
            if (dir.items[i].type == BARE_FT_DIR &&
                strcmp(dir.items[i].name, ".") != 0 &&
                strcmp(dir.items[i].name, "..") != 0) {
                printf("\n%s/%s:\n", path, dir.items[i].name);
                ls_dir(dir.items[i].path);
            }
        }
    }

    bare_dir_free(&dir);
}

int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "ls", "ls [options] [file...]",
                  "List directory contents.");

    bare_cli_add_opt(&cli, (bare_opt_t){ 'a', "all",        "show hidden entries",         BARE_OPT_BOOL,   {.bval=&flag_all},        false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'A', "almost-all", "do not list implicit . and ..", BARE_OPT_BOOL, {.bval=&flag_almost_all}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 0,   "block-size", "scale sizes by SIZE",         BARE_OPT_STRING, {.sval=&opt_block_size}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'B', "ignore-backups", "do not list entries ending with ~", BARE_OPT_BOOL, {.bval=&flag_ignore_backups}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 0,   "zero",      "end each output line with NUL", BARE_OPT_BOOL,   {.bval=&flag_zero},      false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'X', NULL,        "  sort by extension",           BARE_OPT_BOOL,   {.bval=&flag_sort_extension}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'x', NULL,        "  list entries by lines",       BARE_OPT_BOOL,   {.bval=&flag_horizontal}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'R', "recursive",  "list subdirectories recursively", BARE_OPT_BOOL, {.bval=&flag_recursive}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'd', "directory", "list directories themselves, not their contents", BARE_OPT_BOOL, {.bval=&flag_directory}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'l', "long",      "long listing format",         BARE_OPT_BOOL,   {.bval=&flag_long},      false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'h', "human",     "human-readable sizes",        BARE_OPT_BOOL,   {.bval=&flag_human},     false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 't', "sort-time", "sort by modification time",   BARE_OPT_BOOL,   {.bval=&flag_sort_time}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'S', "sort-size", "sort by file size",           BARE_OPT_BOOL,   {.bval=&flag_sort_size}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'r', "reverse",   "reverse sort order",          BARE_OPT_BOOL,   {.bval=&flag_reverse},   false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'c', "color",     "colorize output",             BARE_OPT_BOOL,   {.bval=&flag_color},     false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'F', "classify",  "append indicator (one of */=>@|) to entries", BARE_OPT_BOOL, {.bval=&flag_classify}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ '1', NULL,        "  one entry per line",          BARE_OPT_BOOL,   {.bval=&flag_one},       false });
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help",      "show this help and exit",     BARE_OPT_BOOL,   {.bval=&flag_help},      false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }

    if (!bare_isatty(stdout)) flag_color = false;
    if (cli.npositional == 0) {
        if (flag_directory) {
            bare_entry_t entry = {0};
            bare_err_t stat_err = bare_entry_stat(".", &entry);
            if (stat_err != BARE_OK)
                bare_die_err("ls", stat_err);
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
        } else {
            ls_dir(".");
        }
    } else {
        for (size_t i = 0; i < cli.npositional; i++) {
            bool is_dir = bare_path_is_dir(cli.positional[i]);
            if (cli.npositional > 1 && (!flag_directory || !is_dir))
                printf("%s:\n", cli.positional[i]);
            if (is_dir && !flag_directory) {
                ls_dir(cli.positional[i]);
            } else {
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
