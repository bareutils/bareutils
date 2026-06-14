/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <barelib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <limits.h>
static bool flag_help = false;
static bool flag_context = false;
static bool flag_group = false;
static bool flag_groups = false;
static bool flag_name = false;
static bool flag_real = false;
static bool flag_user = false;
static bool flag_zero = false;
static const char *uid_name(uid_t uid) {
    struct passwd *pw = getpwuid(uid);
    return pw ? pw->pw_name : NULL;
}

static const char *gid_name(gid_t gid) {
    struct group *gr = getgrgid(gid);
    return gr ? gr->gr_name : NULL;
}

static void print_id(gid_t id, const char *(*namefn)(gid_t), bool name) {
    if (name) {
        const char *n = namefn(id);
        printf("%s", n ? n : "");
    } else {
        printf("%u", (unsigned)id);
    }
}

int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "id", "id [OPTION]... [USER]...", "Print user and group information.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'Z', "context", "print only the security context", BARE_OPT_BOOL, {.bval=&flag_context}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'g', "group", "print only the effective group ID", BARE_OPT_BOOL, {.bval=&flag_group}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'G', "groups", "print all group IDs", BARE_OPT_BOOL, {.bval=&flag_groups}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'n', "name", "print a name instead of a number", BARE_OPT_BOOL, {.bval=&flag_name}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'r', "real", "print the real ID instead of the effective ID", BARE_OPT_BOOL, {.bval=&flag_real}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'u', "user", "print only the effective user ID", BARE_OPT_BOOL, {.bval=&flag_user}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'z', "zero", "delimit entries with NUL, not whitespace", BARE_OPT_BOOL, {.bval=&flag_zero}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }

    int n_only = !!flag_user + !!flag_group + !!flag_groups + !!flag_context;
    if (n_only > 1)
        bare_die("id", "cannot print 'only' of more than one");

    if (flag_zero && n_only == 0)
        bare_die("id", "option -z not permitted in default format");

    const char *user = cli.npositional > 0 ? cli.positional[0] : NULL;
    uid_t uid;
    gid_t gid;
    gid_t *groups = NULL;
    int ngroups = 0;
    if (user) {
        struct passwd *pw = getpwnam(user);
        if (!pw) bare_die("id", "no such user");
        uid = pw->pw_uid;
        gid = pw->pw_gid;
        int nalloc = (int)sysconf(_SC_NGROUPS_MAX);
        if (nalloc <= 0) nalloc = 64;
        groups = bare_malloc((size_t)nalloc * sizeof(gid_t));
        if (getgrouplist(user, gid, groups, &nalloc) == -1) {
            groups = bare_realloc(groups, (size_t)nalloc * sizeof(gid_t));
            getgrouplist(user, gid, groups, &nalloc);
        }
        ngroups = nalloc;
    } else {
        uid = flag_real ? getuid() : geteuid();
        gid = flag_real ? getgid() : getegid();
        int nalloc = (int)sysconf(_SC_NGROUPS_MAX);
        if (nalloc <= 0) nalloc = 64;
        groups = bare_malloc((size_t)nalloc * sizeof(gid_t));
        ngroups = getgroups(nalloc, groups);
        if (ngroups == -1) { free(groups); groups = NULL; ngroups = 0; }
    }

    if (flag_user) {
        print_id((gid_t)uid, (const char *(*)(gid_t))uid_name, flag_name);
        putchar(flag_zero ? '\0' : '\n');
    } else if (flag_group) {
        print_id(gid, gid_name, flag_name);
        putchar(flag_zero ? '\0' : '\n');
    } else if (flag_groups) {
        print_id(gid, gid_name, flag_name);
        for (int i = 0; i < ngroups; i++) {
            if (groups[i] == gid) continue;
            putchar(flag_zero ? '\0' : ' ');
            print_id(groups[i], gid_name, flag_name);
        }
        putchar(flag_zero ? '\0' : '\n');
    } else if (flag_context) {
        int cfd = open("/proc/self/attr/current", O_RDONLY);
        if (cfd == -1)
            bare_die("id", "--context works only on an SELinux-enabled kernel");
        char ctx[4096];
        ssize_t n = read(cfd, ctx, sizeof(ctx) - 1);
        close(cfd);
        if (n <= 0) bare_die("id", "cannot read security context");
        ctx[n] = '\0';
        printf("%s\n", ctx);
    } else {
        uid_t puid = uid;
        gid_t pgid = gid;
        printf("uid=%u", (unsigned)puid);
        const char *un = uid_name(puid);
        if (un) printf("(%s)", un);
        printf(" gid=%u", (unsigned)pgid);
        const char *gn = gid_name(pgid);
        if (gn) printf("(%s)", gn);
        printf(" groups=%u", (unsigned)pgid);
        if (gn) printf("(%s)", gn);
        for (int i = 0; i < ngroups; i++) {
            if (groups[i] == pgid) continue;
            printf(",%u", (unsigned)groups[i]);
            const char *gn2 = gid_name(groups[i]);
            if (gn2) printf("(%s)", gn2);
        }
        printf("\n");
    }

    free(groups);
    bare_cli_free(&cli);
    return 0;
}
