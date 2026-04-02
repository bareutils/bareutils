/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <barextras.h>
#include <barelib.h>

// neo: enable GNU extensions only for GNU Libc and only for sched.h
#ifdef __GLIBC__
#define __USE_GNU
#endif
#include <sched.h>
#undef __USE_GNU

static bool flag_help = false;
static long int flag_ignore = 0;
static bool flag_all = false;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "nproc", "nproc [OPTION]...", "Print the number of processing units available to the current process.\nwhich may be less than the number of online processors");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'a', "all", "print the number of installed processors", BARE_OPT_BOOL, {.bval=&flag_all}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'i', "ignore", "if possible, exclude N processing units", BARE_OPT_INT, {.ival=&flag_ignore}, 0});
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }
    int count = 0;
	unsigned long mask[1024];
    if (flag_all) {
        DIR *cpusd = opendir("/sys/devices/system/cpu");
        if (cpusd) {
            struct dirent *de;
            while (NULL != (de = readdir(cpusd))) {
                char *cpuid = strstr(de->d_name, "cpu");
                if (cpuid && isdigit(cpuid[strlen(cpuid) - 1])) count++;
            }
            closedir(cpusd);
        } else {bare_die_err("nproc", BARE_ENOENT);}
    } else {
	    if (sched_getaffinity(0, sizeof(mask), (void*)mask) == 0) {
		    unsigned int i;
		    for (i = 0; i < ARRAY_SIZE(mask); i++) {
			    unsigned long m = mask[i];
			    while (m) {
				    if (m & 1)
					    count++;
				    m >>= 1;
			    }
		    }
	    }
    }
    if (flag_ignore < count) { 
        if (flag_ignore < 0) {
            char* msg = "invalid number: '";
            char* err = bare_malloc(strlen(msg) + sizeof(flag_ignore) + 1);
            sprintf(err, "%s%ld'", msg, flag_ignore);
            bare_die("nproc", err);
        } 
        count -= flag_ignore;
    } else {
        count = 1; 
    }
    printf("%u\n", count);
    bare_cli_free(&cli);
    return 0;
}
