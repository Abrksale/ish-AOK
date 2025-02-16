#ifndef TASK_H
#define TASK_H

#include <pthread.h>
#include "emu/cpu.h"
#include "kernel/mm.h"
#include "kernel/fs.h"
#include "kernel/signal.h"
#include "kernel/resource.h"
#include "fs/sockrestart.h"
#include "util/list.h"
#include "util/timer.h"
#include "util/sync.h"

struct task {
    struct cpu_state cpu;
    struct mm *mm; // locked by general_lock
    struct mem *mem; // pointer to mm.mem, for convenience
    pthread_t thread;
    uint64_t threadid;

    bool process_info_being_read; // Set when something like ps, top, etc wants to access task info. -mke

    pthread_mutex_t death_lock; // Set when process is about to be reaped.  Immediately cease all activity on this task.  -mke
    
    struct {
        pthread_mutex_t lock;
        int count; // If positive, don't delete yet, wait_to_delete -mke
    } critical_region;
    
    struct {
        pthread_mutex_t lock;
        int count; // Count of locks held by current task =mke
    } locks_held;
    
    int stuck_count;

    struct tgroup *group; // immutable
    struct list group_links;
    pid_t_ pid, tgid; // immutable
    uid_t_ uid, gid;
    uid_t_ euid, egid;
    uid_t_ suid, sgid;
#define MAX_GROUPS 32
    unsigned ngroups;
    uid_t_ groups[MAX_GROUPS];
    char comm[16] __strncpy_safe; // locked by general_lock
    bool did_exec; // for that one annoying setsid edge case

    struct fdtable *files;
    struct fs_info *fs;

    // locked by sighand->lock
    struct sighand *sighand;
    sigset_t_ blocked;
    sigset_t_ pending;
    sigset_t_ waiting; // if nonzero, an ongoing call to sigtimedwait is waiting on these
    struct list queue;
    cond_t pause; // please don't signal this
    // private
    sigset_t_ saved_mask;
    bool has_saved_mask;

    struct {
        // Locks all ptrace-related things
        lock_t lock;
        cond_t cond;

        bool traced;
        bool stopped;
        bool sysgood;
        bool stop_at_syscall;
        bool syscall_stopped;
        int signal;
        struct siginfo_ info;
        int trap_event;
        int syscall;
    } ptrace;

    // locked by pids_lock
    struct task *parent;
    struct list children;
    struct list siblings;

    addr_t clear_tid;
    addr_t robust_list;

    // locked by pids_lock
    dword_t exit_code;
    bool zombie;
    bool exiting;
    bool io_block;

    // this structure is allocated on the stack of the parent's clone() call
    struct vfork_info {
        bool done;
        cond_t cond;
        lock_t lock;
    } *vfork;
    int exit_signal;

    // lock for anything that needs locking but is not covered by some other lock
    // specifically: comm, mm
    lock_t general_lock;

    struct task_sockrestart sockrestart;

    // current condition/lock, so it can be notified in case of a signal
    cond_t *waiting_cond;
    lock_t *waiting_lock;
    lock_t waiting_cond_lock;
};

// current will always give the process that is currently executing
// if I have to stop using __thread, current will become a macro
extern __thread struct task *current;

static inline void task_set_mm(struct task *task, struct mm *mm) {
    task->mm = mm;
    task->mem = &task->mm->mem;
    task->cpu.mmu = &task->mem->mmu;
}

// Creates a new process, initializes most fields from the parent. Specify
// parent as NULL to create the init process. Returns NULL if out of memory.
// Ends with an underscore because there's a mach function by the same name
struct task *task_create_(struct task *parent);
// Removes the process from the process table and frees it. Must be called with pids_lock.
void task_destroy(struct task *task);

// misc
void vfork_notify(struct task *task);
pid_t_ task_setsid(struct task *task);
void task_leave_session(struct task *task);

struct posix_timer {
    struct timer *timer;
    int_t timer_id;
    struct task *task;
    int_t signal;
    union sigval_ sig_value;
};

// struct thread_group is way too long to type comfortably
struct tgroup {
    struct list threads; // locked by pids_lock, by majority vote
    struct task *leader; // immutable
    long group_count_in_int;
    struct rusage_ rusage;

    // locked by pids_lock
    pid_t_ sid, pgid;
    struct list session;
    struct list pgroup;

    bool stopped;
    cond_t stopped_cond;

    struct tty *tty;
    struct timer *itimer;
#define TIMERS_MAX 16
    struct posix_timer posix_timers[TIMERS_MAX];

    struct rlimit_ limits[RLIMIT_NLIMITS_];

    // https://twitter.com/tblodt/status/957706819236904960
    // TODO locking
    bool doing_group_exit;
    dword_t group_exit_code;

    struct rusage_ children_rusage;
    cond_t child_exit;

    dword_t personality;

    // for everything in this struct not locked by something else
    lock_t lock;
};

static inline bool task_is_leader(struct task *task) {
    return task->group->leader == task;
}

struct pid {
    dword_t id;
    struct task *task;
    struct list alive; // list of alive pids
    struct list session;
    struct list pgroup;
};

// @alive_pids_list is used as a head of all active pids.
// Scanning this list, you should start list_for_each from alive_pids_list,
// to avoid having this head element in your cycle.
extern struct list alive_pids_list;

// synchronizes obtaining a pointer to a task and freeing that task
extern lock_t pids_lock;
// these functions must be called with pids_lock
struct pid *pid_get(dword_t pid);
struct pid *pid_get_last_allocated(void);
struct task *pid_get_task(dword_t pid);
struct task *pid_get_task_zombie(dword_t id); // don't return null if the task exists as a zombie

dword_t get_count_of_blocked_tasks(void);
dword_t get_count_of_alive_tasks(void);
void zero_critical_regions_count(void);

#define MAX_PID (1 << 15) // oughta be enough

// TODO document
void task_start(struct task *task);
void task_run_current(void);

extern void (*exit_hook)(struct task *task, int code);

#define superuser() (current != NULL && current->euid == 0)

// Update the thread name to match the current task, in the format "comm-pid".
// Will ensure that the -pid part always fits, then will fit as much of comm as possible.
void update_thread_name(void);

// To collect statics on which tasks are blocked we need to proccess areas
// of code which could block our task (e.g reads or writes). Before executing
// of functions which can block the task, we mark our task as blocked and
// unblock it after the function is executed.
__attribute__((always_inline)) inline int task_may_block_start(void) {
    modify_critical_region_counter_wrapper(1, __FILE__, __LINE__);
    current->io_block = 1;
    return 0;
}

__attribute__((always_inline)) inline int task_may_block_end(void) {
    current->io_block = 0;
    modify_critical_region_counter_wrapper(-1, __FILE__, __LINE__);
    return 0;
}

#define TASK_MAY_BLOCK for (int i = task_may_block_start(); i < 1; task_may_block_end(), i++)

#endif
