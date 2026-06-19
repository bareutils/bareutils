/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/auxv.h>
#include <barelib.h>
static const char *cpuinfo_val(const char *key, char *buf, size_t bufsz) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return NULL;
    char line[512];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0) {
            char *p = line + klen;
            while (*p == ' ' || *p == '\t') p++;
            if (*p != ':') continue;
            p++;
            while (*p == ' ' || *p == '\t') p++;
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            strncpy(buf, p, bufsz - 1);
            buf[bufsz - 1] = '\0';
            fclose(f);
            return buf;
        }
    }
    fclose(f);
    return NULL;
}
static bool flag_help = false;
static bool flag_all = false;
static bool flag_kernel_name = false;
static bool flag_nodename = false;
static bool flag_kernel_release = false;
static bool flag_kernel_version = false;
static bool flag_machine = false;
static bool flag_processor = false;
static bool flag_hardware_platform = false;
static bool flag_operating_system = false;
int main(int argc, char **argv) {
    bare_cli_t cli;
    bare_cli_init(&cli, "uname", "uname [OPTION]...", "Print certain system information. With no OPTION, same as -s.");
    bare_cli_add_opt(&cli, (bare_opt_t){ '?', "help", "show this help and exit", BARE_OPT_BOOL, {.bval=&flag_help}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'a', "all", "print all information, in the following order,\nexcept omit -p and -i if unknown", BARE_OPT_BOOL, {.bval=&flag_all}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 's', "kernel-name", "print the kernel name", BARE_OPT_BOOL, {.bval=&flag_kernel_name}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'n', "nodename", "print the network node hostname", BARE_OPT_BOOL, {.bval=&flag_nodename}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'r', "kernel-release", "print the kernel release", BARE_OPT_BOOL, {.bval=&flag_kernel_release}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'v', "kernel-version", "print the kernel version", BARE_OPT_BOOL, {.bval=&flag_kernel_version}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'm', "machine", "print the machine hardware name", BARE_OPT_BOOL, {.bval=&flag_machine}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'p', "processor", "print the processor type (non-portable)", BARE_OPT_BOOL, {.bval=&flag_processor}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'i', "hardware-platform", "print the hardware platform (non-portable)", BARE_OPT_BOOL, {.bval=&flag_hardware_platform}, false });
    bare_cli_add_opt(&cli, (bare_opt_t){ 'o', "operating-system", "print the operating system", BARE_OPT_BOOL, {.bval=&flag_operating_system}, false });
    bare_cli_parse(&cli, argc, argv);
    if (flag_help) {
        bare_cli_help(&cli);
        printf("- bareutils %s\n", BAREUTILS_VERSION);
        bare_cli_free(&cli);
        return 0;
    }
    struct utsname u;
    if (uname(&u) < 0) {
        bare_die_errno("uname", "uname failed");
    }
    bool any = flag_all || flag_kernel_name || flag_nodename || flag_kernel_release
            || flag_kernel_version || flag_machine || flag_processor
            || flag_hardware_platform || flag_operating_system;
    if (!any)
        flag_kernel_name = true;
    int n = 0;
    if (flag_kernel_name || flag_all) n += printf("%s%s", n ? " " : "", u.sysname);
    if (flag_nodename || flag_all)   n += printf("%s%s", n ? " " : "", u.nodename);
    if (flag_kernel_release || flag_all) n += printf("%s%s", n ? " " : "", u.release);
    if (flag_kernel_version || flag_all) n += printf("%s%s", n ? " " : "", u.version);
    if (flag_machine || flag_all)    n += printf("%s%s", n ? " " : "", u.machine);
    char proc_buf[256], hwplat_buf[256];
    const char *proc = cpuinfo_val("model name", proc_buf, sizeof(proc_buf));
    if (!proc) proc = "unknown";
    const char *hwplat = cpuinfo_val("vendor_id", hwplat_buf, sizeof(hwplat_buf));
    if (!hwplat) hwplat = "unknown";
    if (flag_processor || (flag_all && strcmp(proc, "unknown") != 0))
        n += printf("%s%s", n ? " " : "", proc);
    if (flag_hardware_platform || (flag_all && strcmp(hwplat, "unknown") != 0))
        n += printf("%s%s", n ? " " : "", hwplat);
    const char *os = u.sysname;
    if (strcmp(os, "Linux") == 0) os = "GNU/Linux"; //neo: speak the truth!
    if (flag_operating_system || flag_all) n += printf("%s%s", n ? " " : "", os);
    if (n)
        printf("\n");
    bare_cli_free(&cli);
    return 0;
}
