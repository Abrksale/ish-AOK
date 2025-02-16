#include <sys/stat.h>
#include <inttypes.h>
#include <string.h>
#include "kernel/calls.h"
#include "kernel/task.h"
#include "kernel/resource_locking.h"
#include "fs/proc.h"
#include "fs/proc/net.h"
#include "platform/platform.h"
#include <sys/param.h> // for MIN and MAX
#include "emu/cpuid.h"

static int proc_show_version(struct proc_entry *UNUSED(entry), struct proc_data *buf) {
    struct uname uts;
    do_uname(&uts);
    proc_printf(buf, "%s version %s %s\n", uts.system, uts.release, uts.version);
    return 0;
}

void parse_edx_flags(dword_t edx, char *edx_flags) { /* Translate edx bit flags into text */
    static const char *enumerated[] = {
        "fpu ", "vme ", "de ", "pse ", "tsc ", "msr ", "pae ", "mce ", "cx8 ", "apic ", "Reserved ",
        "sep ", "mtrr ", "pge ", "mca ", "cmov ", "", "pse-36 ", "psn ", "clfsh ", "Reserved ",
        "ds ", "acpi ", "mmx ", "fxsr ", "sse ", "sse2 ", "ss ", "htt ", "tm ", "Reserved ", "pbe "
    };

    size_t offset = 0;
    for (size_t i = 0; i < 32; i++) {
        if (edx & (1 << i)) {
            const size_t enum_len = strlen(enumerated[i]);
            memcpy(edx_flags + offset, enumerated[i], enum_len);
            offset += enum_len;
        }
    }
    edx_flags[offset] = '\0';
}

static void unpack32(dword_t src, void *dst) {
    unsigned char *p = dst;
    for (size_t i = 0; i < 4; i++) {
        p[i] = (src >> (0x08 * i)) & 0xff;
    }
}

void translate_vendor_id(char *buf, dword_t *ebx, dword_t *ecx, dword_t *edx) {
    unpack32(*ebx, &buf[0]);
    unpack32(*edx, &buf[4]);
    unpack32(*ecx, &buf[8]);
}

static int proc_show_cpuinfo(struct proc_entry *UNUSED(entry), struct proc_data *buf) {
    dword_t eax = 0;
    dword_t ebx;
    dword_t ecx;
    dword_t edx;

    do_cpuid(&eax, &ebx, &ecx, &edx); // Get vendor_id

    char vendor_id[13] = { 0 };
    translate_vendor_id(vendor_id, &ebx, &ecx, &edx);

    eax = 1;
    do_cpuid(&eax, &ebx, &ecx, &edx);

    // static const char len[] = {
    //     "fpu " "vme " "de " "pse " "tsc " "msr " "pae " "mce " "cx8 " "apic " "Reserved "
    //     "sep " "mtrr " "pge " "mca " "cmov " "" "pse-36 " "psn " "clfsh " "Reserved "
    //     "ds " "acpi " "mmx " "fxsr " "sse " "sse2 " "ss " "htt " "tm " "Reserved " "pbe "
    // };

    char edx_flags[148] = { 0 };
    parse_edx_flags(edx, edx_flags);

    int cpu_count = get_cpu_count(); // One entry per device processor
    int i;

    for( i=0; i<cpu_count ; i++ ) {
        proc_printf(buf, "processor       : %d\n",i);
        proc_printf(buf, "vendor_id       : %s\n", vendor_id);
        proc_printf(buf, "cpu family      : %d\n",1);
        proc_printf(buf, "model           : %d\n",1);
        proc_printf(buf, "model name      : iSH Virtual i686-compatible CPU @ 1.066GHz\n");
        proc_printf(buf, "stepping        : %d\n",1);
        proc_printf(buf, "CPU MHz         : 1066.00\n");
        proc_printf(buf, "cache size      : %d kb\n",0);
        proc_printf(buf, "pysical id      : %d\n",0);
        proc_printf(buf, "siblings        : %d\n",0);
        proc_printf(buf, "core id         : %d\n",0);
        proc_printf(buf, "cpu cores       : %d\n",cpu_count);
        proc_printf(buf, "apicid          : %d\n",0);
        proc_printf(buf, "initial apicid  : %d\n",0);
        proc_printf(buf, "fpu             : yes\n");
        proc_printf(buf, "fpu_exception   : yes\n");
        proc_printf(buf, "cpuid level     : %d\n",13);
        proc_printf(buf, "wp              : yes\n");
        proc_printf(buf, "flags           : %s\n", edx_flags); // Pulled from do_cpuid
        proc_printf(buf, "bogomips        : 1066.00\n");
        proc_printf(buf, "clflush size    : %d\n", ebx);
        proc_printf(buf, "cache_alignment : %d\n",64);
        proc_printf(buf, "address sizes   : 36 bits physical, 32 bits virtual\n");
        proc_printf(buf, "power management:\n");
        proc_printf(buf, "\n");
    }

    return 0;
}

static int proc_show_stat(struct proc_entry *UNUSED(entry), struct proc_data *buf) {
    int ncpus = get_cpu_count();
    struct cpu_usage total_usage = get_total_cpu_usage();
    struct cpu_usage* per_cpu_usage = 0;
    struct uptime_info uptime_info = get_uptime();
    unsigned uptime = uptime_info.uptime_ticks;
    
    proc_printf(buf, "cpu  %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" 0 0 0 0\n", total_usage.user_ticks, total_usage.nice_ticks, total_usage.system_ticks, total_usage.idle_ticks);
    
    int err = get_per_cpu_usage(&per_cpu_usage);
    if (!err) {
        for (int i = 0; i < ncpus; i++) {
            proc_printf(buf, "cpu%d  %"PRIu64" %"PRIu64" %"PRIu64" %"PRIu64" 0 0 0 0\n", i, per_cpu_usage[i].user_ticks, per_cpu_usage[i].nice_ticks, per_cpu_usage[i].system_ticks, per_cpu_usage[i].idle_ticks);
        }
        free(per_cpu_usage);
    }
    
    int blocked_task_count = get_count_of_blocked_tasks();
    int alive_task_count = get_count_of_alive_tasks();
    proc_printf(buf, "ctxt 0\n");
    proc_printf(buf, "btime %u\n", uptime);
    proc_printf(buf, "processes %d\n", alive_task_count);
    proc_printf(buf, "procs_running %d\n", alive_task_count - blocked_task_count);
    proc_printf(buf, "procs_blocked %d\n", blocked_task_count);
    
    return 0;
}

static void show_kb(struct proc_data *buf, const char *name, uint64_t value) {
    proc_printf(buf, "%s%8"PRIu64" kB\n", name, value / 1000);
}

static int proc_show_filesystems(struct proc_entry *UNUSED(entry), struct proc_data *buf) {
    char *filesystems = get_filesystems();
    proc_printf(buf, "%s", filesystems);
    free(filesystems);
    return 0;
}

static int proc_show_meminfo(struct proc_entry *UNUSED(entry), struct proc_data *buf) {
    struct mem_usage usage = get_mem_usage();
    show_kb(buf, "MemTotal:       ", usage.total);
    show_kb(buf, "MemFree:        ", usage.free);
    show_kb(buf, "MemAvailable:   ", usage.available);
    show_kb(buf, "MemShared:      ", usage.free);
    show_kb(buf, "Active:         ", usage.active);
    show_kb(buf, "Inactive:       ", usage.inactive);
    show_kb(buf, "SwapCached:     ", 0);
    // a bunch of crap busybox top needs to see or else it gets stack garbage
    show_kb(buf, "Shmem:          ", 0);
    show_kb(buf, "Buffers:        ", 0);
    show_kb(buf, "Cached:         ", usage.cached);
    show_kb(buf, "SwapTotal:      ", 0);
    show_kb(buf, "SwapFree:       ", 0);
    show_kb(buf, "Dirty:          ", 0);
    show_kb(buf, "Writeback:      ", 0);
    show_kb(buf, "AnonPages:      ", 0);
    show_kb(buf, "Mapped:         ", 0);
    show_kb(buf, "Slab:           ", 0);
    // Stuff that doesn't map elsehwere
    show_kb(buf, "Swapins:        ", usage.swapins);
    show_kb(buf, "Swapouts:       ", usage.swapouts);
    show_kb(buf, "WireCount:      ", usage.wirecount);
    return 0;
}

static int proc_show_uptime(struct proc_entry *UNUSED(entry), struct proc_data *buf) {
    struct uptime_info uptime_info = get_uptime();
    unsigned uptime = uptime_info.uptime_ticks;
    proc_printf(buf, "%u.%u %u.%u\n", uptime / 100, uptime % 100, uptime / 100, uptime % 100);
    return 0;
}
static int proc_show_vmstat(struct proc_entry *UNUSED(entry), struct proc_data *UNUSED(buf)) {
    return 0;
}
/*
 8       0 sda 52553 537 6661171 8035 394441 324883 29295529 405166 0 111828 240028 0 0 0 0
 8       1 sda1 421 0 9657 21 2 0 9 0 0 16 20 0 0 0 0
 8       2 sda2 51958 537 6642610 7999 392133 324883 29295520 405043 0 111804 239984 0 0 0 0
 8       3 sda3 70 0 4592 6 0 0 0 0 0 8 12 0 0 0 0
11       0 sr0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
 */
static int proc_show_diskstats(struct proc_entry *UNUSED(entry), struct proc_data *buf) {
    //proc_printf(buf, "8       0 disk1 52553 537 6661171 8035 394441 324883 29295529 405166 0 111828 240028 0 0 0 0\n");
    proc_printf(buf, "8       0 disk1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n");
    //proc_printf(buf, "8       0 sda1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n");
    //proc_printf(buf, "8       0 sda2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n");
    //proc_printf(buf, "8       0 sda3 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n");
    return 0;
}

static int proc_show_loadavg(struct proc_entry *UNUSED(entry), struct proc_data *buf) {
    struct uptime_info uptime = get_uptime();
    struct pid *last_pid = pid_get_last_allocated();
    int last_pid_id = last_pid ? last_pid->id : 0;
    double load_1m = uptime.load_1m / 65536.0;
    double load_5m = uptime.load_5m / 65536.0;
    double load_15m = uptime.load_15m / 65536.0;
    int blocked_task_count = get_count_of_blocked_tasks();
    int alive_task_count = get_count_of_alive_tasks();
    // running_task_count is calculated abool proc_net_readdir(struct proc_entry * UNUSED(entry), unsigned long *index, struct proc_entry *next_entry) pproximetly, since we don't know the real number of currently running tasks.
    int running_task_count = MIN(get_cpu_count(), (int)(alive_task_count - blocked_task_count));
    proc_printf(buf, "%.2f %.2f %.2f %u/%u %u\n", load_1m, load_5m, load_15m, running_task_count, alive_task_count, last_pid_id);
    return 0;
}

static int proc_readlink_self(struct proc_entry *UNUSED(entry), char *buf) {
    sprintf(buf, "%d/", current->pid);
    return 0;
}

static void proc_print_escaped(struct proc_data *buf, const char *str) {
    for (size_t i = 0; str[i]; i++) {
        switch (str[i]) {
            case '\t': case ' ': case '\\':
                proc_printf(buf, "\\%03o", str[i]);
                break;
            default:
                proc_printf(buf, "%c", str[i]);
        }
    }
}

#define proc_printf_comma(buf, at_start, format, ...) do { \
    proc_printf((buf), "%s" format, *(at_start) ? "" : ",", ##__VA_ARGS__); \
    *(at_start) = false; \
} while (0)

static int proc_show_mounts(struct proc_entry *UNUSED(entry), struct proc_data *buf) {
    struct mount *mount;
    list_for_each_entry(&mounts, mount, mounts) {
        const char *point = mount->point;
        if (point[0] == '\0')
            point = "/";

        proc_print_escaped(buf, mount->source);
        proc_printf(buf, " ");
        proc_print_escaped(buf, point);
        proc_printf(buf, " %s ", mount->fs->name);
        bool at_start = true;
        proc_printf_comma(buf, &at_start, "%s", mount->flags & MS_READONLY_ ? "ro" : "rw");
        if (mount->flags & MS_NOSUID_)
            proc_printf_comma(buf, &at_start, "nosuid");
        if (mount->flags & MS_NODEV_)
            proc_printf_comma(buf, &at_start, "nodev");
        if (mount->flags & MS_NOEXEC_)
            proc_printf_comma(buf, &at_start, "noexec");
        if (strcmp(mount->info, "") != 0)
            proc_printf_comma(buf, &at_start, "%s", mount->info);
        proc_printf(buf, " 0 0\n");
    };
    return 0;
}

// in alphabetical order
struct proc_dir_entry proc_root_entries[] = {
    {"cpuinfo", .show = proc_show_cpuinfo},
    {"diskstats", .show = proc_show_diskstats},
    {"filesystems", .show = proc_show_filesystems},
    {"ish", S_IFDIR, .children = &proc_ish_children},
    {"loadavg", .show = proc_show_loadavg},
    {"meminfo", .show = proc_show_meminfo},
    {"mounts", .show = proc_show_mounts},
    {"net", S_IFDIR, .children = &proc_net_children},
    {"self", S_IFLNK, .readlink = proc_readlink_self},
    {"stat", .show = &proc_show_stat},
    {"sys", S_IFDIR, .children = &proc_sys_children},
    {"uptime", .show = proc_show_uptime},
    {"version", .show = proc_show_version},
    {"vmstat", .show = proc_show_vmstat},
};
#define PROC_ROOT_LEN sizeof(proc_root_entries)/sizeof(proc_root_entries[0])

static bool proc_root_readdir(struct proc_entry *UNUSED(entry), unsigned long *index, struct proc_entry *next_entry) {
    if (*index < PROC_ROOT_LEN) {
        *next_entry = (struct proc_entry) {&proc_root_entries[*index], *index, NULL, NULL, 0, 0};
        (*index)++;
        return true;
    }

    pid_t_ pid = *index - PROC_ROOT_LEN;
    if (pid <= MAX_PID) {
        modify_critical_region_counter(current, 1, __FILE__, __LINE__);
        //lock(&pids_lock, 0);
        do {
            pid++;
        } while (pid <= MAX_PID && pid_get_task(pid) == NULL);
        //unlock_pids(&pids_lock);
        modify_critical_region_counter(current, -1, __FILE__, __LINE__);
        if (pid > MAX_PID) {
            return false;
        }
        *next_entry = (struct proc_entry) {&proc_pid, .pid = pid};
        *index = pid + PROC_ROOT_LEN;
        //modify_critical_region_counter(current, -1, __FILE__, __LINE__);
        return true;
    }

    return false;
}

struct proc_dir_entry proc_root = {NULL, S_IFDIR, .readdir = proc_root_readdir};
