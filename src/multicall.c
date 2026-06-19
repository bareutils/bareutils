/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <string.h>
#include <stdio.h>
#include <barelib.h>
#include "applets.h"
int main(int argc, char **argv) {
    if (argc < 1 || !argv[0]) {
        fprintf(stderr, "no argv[0]\n");
        return 1;
    }
    const char *prog = argv[0];
    const char *slash = strrchr(prog, '/');
    if (slash)
        prog = slash + 1;
    if (strcmp(prog, "bareutils") == 0 || strcmp(prog, "multicall") == 0) {
        if (argc > 1) {
            for (const applet_t *a = applets; a->name; a++) {
                if (strcmp(argv[1], a->name) == 0)
                    return a->fn(argc - 1, argv + 1);
            }
            fprintf(stderr, "bareutils: unknown applet '%s'\n", argv[1]);
            return 1;
        }
        unsigned napplets = 0;
        while (applets[napplets].name)
            napplets++;
        printf("bareutils v%s multi-call binary.\n", BAREUTILS_VERSION);
        printf("Licensed under MPL 2.0. See source distribution for license details.\n");
        printf("\nUsage: bareutils [function [arguments]...]\n");
        printf("   or: function [arguments]...\n");
        printf("\nCurrently available applets:\n");
        int width = bare_term_width();
        int ncols = width < 80 ? 1 : width / 18;
        int nrows = (napplets + ncols - 1) / ncols;
        for (int row = 0; row < nrows; row++) {
            for (int col = 0; col < ncols; col++) {
                unsigned idx = col * nrows + row;
                if (idx < napplets)
                    printf("  %-15s", applets[idx].name);
            }
            printf("\n");
        }
        return 0;
    }
    for (const applet_t *a = applets; a->name; a++) {
        if (strcmp(prog, a->name) == 0)
            return a->fn(argc, argv);
    }
    fprintf(stderr, "%s: unknown applet\n", prog);
    return 1;
}
