#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Trabelsi");
MODULE_DESCRIPTION("AI-OS: Smart OOM Guard v1.0");

#define MAX_PROTECTED     32
#define PROC_GUARD        "ai_oom_guard"
#define PROC_CHECKPOINT   "ai_checkpoint"
#define SCAN_INTERVAL     (5 * HZ)
#define MIN_RUNTIME_SECS  5
#define MIN_RAM_MB        200
#define AIOS_VERSION      "1.0.0"

static const char* AI_NAMES[] = {
    "python", "python3", "training_sim",
    "jupyter", "nvcc", NULL
};

typedef struct {
    pid_t  pid;
    char   name[64];
    u64    start_time;
    u64    last_checkpoint;
    int    checkpoint_count;
    int    manual;
} ProtectedProcess;

static ProtectedProcess protected[MAX_PROTECTED];
static int              protected_count = 0;
static DEFINE_SPINLOCK(protected_lock);
static struct timer_list scan_timer;

static int is_ai_process(const char* name) {
    int i;
    for (i = 0; AI_NAMES[i] != NULL; i++)
        if (strncmp(name, AI_NAMES[i], strlen(AI_NAMES[i])) == 0)
            return 1;
    return 0;
}

static int already_protected(pid_t pid) {
    int i;
    for (i = 0; i < protected_count; i++)
        if (protected[i].pid == pid) return 1;
    return 0;
}

static long get_process_ram_mb(struct task_struct* task) {
    if (!task->mm) return 0;
    return (task->mm->total_vm << PAGE_SHIFT) >> 20;
}

static u64 get_process_runtime_secs(struct task_struct* task) {
    u64 now = ktime_get_ns();
    return (now - task->start_time) / NSEC_PER_SEC;
}

static void do_protect(struct task_struct* task, int manual) {
    ProtectedProcess* p;
    unsigned long flags;

    spin_lock_irqsave(&protected_lock, flags);

    if (already_protected(task->pid) ||
        protected_count >= MAX_PROTECTED) {
        spin_unlock_irqrestore(&protected_lock, flags);
        return;
    }

    task->signal->oom_score_adj = OOM_SCORE_ADJ_MIN / 2;

    p = &protected[protected_count++];
    p->pid              = task->pid;
    p->last_checkpoint  = 0;
    p->checkpoint_count = 0;
    p->start_time       = ktime_get_ns();
    p->manual           = manual;
    strncpy(p->name, task->comm, sizeof(p->name) - 1);

    spin_unlock_irqrestore(&protected_lock, flags);

    printk(KERN_INFO "AI-OS: %s PID %d (%s) OOM protected\n",
           manual ? "MANUAL" : "AUTO", task->pid, task->comm);
}

static void protect_task_auto(struct task_struct* task) {
    long ram_mb  = get_process_ram_mb(task);
    u64  runtime = get_process_runtime_secs(task);

    if (ram_mb < MIN_RAM_MB || runtime < MIN_RUNTIME_SECS) {
        printk(KERN_DEBUG
               "AI-OS: Skip PID %d (%s) RAM:%ldMB t:%llus\n",
               task->pid, task->comm, ram_mb, runtime);
        return;
    }
    do_protect(task, 0);
}

static void scan_and_protect(void) {
    struct task_struct* task;
    rcu_read_lock();
    for_each_process(task)
        if (is_ai_process(task->comm))
            if (!already_protected(task->pid))
                protect_task_auto(task);
    rcu_read_unlock();
}

static void cleanup_dead(void) {
    int i, count = 0;
    ProtectedProcess tmp[MAX_PROTECTED];
    unsigned long flags;

    spin_lock_irqsave(&protected_lock, flags);
    for (i = 0; i < protected_count; i++) {
        struct task_struct* t;
        rcu_read_lock();
        t = pid_task(find_vpid(protected[i].pid), PIDTYPE_PID);
        rcu_read_unlock();
        if (t) {
            tmp[count++] = protected[i];
        } else {
            printk(KERN_INFO
                   "AI-OS: PID %d ended (%d checkpoints)\n",
                   protected[i].pid,
                   protected[i].checkpoint_count);
        }
    }
    memcpy(protected, tmp, count * sizeof(ProtectedProcess));
    protected_count = count;
    spin_unlock_irqrestore(&protected_lock, flags);
}

static void timer_callback(struct timer_list* t) {
    scan_and_protect();
    cleanup_dead();
    mod_timer(&scan_timer, jiffies + SCAN_INTERVAL);
}

// /proc/ai_checkpoint
static ssize_t checkpoint_write(struct file* file,
                                 const char __user* buf,
                                 size_t count, loff_t* pos) {
    char kbuf[32];
    pid_t pid;
    int   chk_num;
    int   i;
    unsigned long flags;

    if (count > sizeof(kbuf) - 1) return -EINVAL;
    if (copy_from_user(kbuf, buf, count)) return -EFAULT;
    kbuf[count] = '\0';
    if (sscanf(kbuf, "%d %d", &pid, &chk_num) != 2)
        return -EINVAL;

    spin_lock_irqsave(&protected_lock, flags);
    for (i = 0; i < protected_count; i++) {
        if (protected[i].pid == pid) {
            protected[i].last_checkpoint  = ktime_get_ns();
            protected[i].checkpoint_count = chk_num;
            break;
        }
    }
    spin_unlock_irqrestore(&protected_lock, flags);

    printk(KERN_INFO "AI-OS: PID %d checkpoint %d\n",
           pid, chk_num);
    return count;
}

static ssize_t checkpoint_read(struct file* file,
                                char __user* buf,
                                size_t count, loff_t* pos) {
    char kbuf[512];
    int  len = 0;
    int  i;
    unsigned long flags;

    if (*pos > 0) return 0;

    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "AI-OS Checkpoint Status\n");
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "=======================\n");

    spin_lock_irqsave(&protected_lock, flags);
    for (i = 0; i < protected_count; i++) {
        u64 secs = 0;
        if (protected[i].last_checkpoint > 0)
            secs = (ktime_get_ns() -
                    protected[i].last_checkpoint)
                    / NSEC_PER_SEC;
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "PID:%-8d NAME:%-16s "
                        "CHECKPOINTS:%-4d LAST:%llus ago\n",
                        protected[i].pid,
                        protected[i].name,
                        protected[i].checkpoint_count,
                        secs);
    }
    if (protected_count == 0)
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "No protected processes\n");
    spin_unlock_irqrestore(&protected_lock, flags);

    if (copy_to_user(buf, kbuf, len)) return -EFAULT;
    *pos = len;
    return len;
}

// /proc/ai_oom_guard
static ssize_t guard_read(struct file* file,
                           char __user* buf,
                           size_t count, loff_t* pos) {
    char kbuf[1024];
    int  len = 0;
    int  i;
    unsigned long flags;

    if (*pos > 0) return 0;

    cleanup_dead();

    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "AI-OS OOM Guard v%s\n", AIOS_VERSION);
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "====================\n");
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "Auto-detect: RAM>%dMB + runtime>%ds\n",
                    MIN_RAM_MB, MIN_RUNTIME_SECS);
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "Protected:   %d/%d\n\n",
                    protected_count, MAX_PROTECTED);

    spin_lock_irqsave(&protected_lock, flags);
    if (protected_count == 0) {
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "No AI processes protected yet\n");
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "Usage: echo <pid> > /proc/%s\n",
                        PROC_GUARD);
    } else {
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "%-8s %-16s %-8s %-8s %-8s\n",
                        "PID", "NAME", "OOM_ADJ",
                        "CHKPTS", "TYPE");
        rcu_read_lock();
        for (i = 0; i < protected_count; i++) {
            struct task_struct* task;
            task = pid_task(find_vpid(protected[i].pid),
                            PIDTYPE_PID);
            if (task)
                len += snprintf(kbuf + len,
                                sizeof(kbuf) - len,
                                "%-8d %-16s %-8d %-8d %-8s\n",
                                protected[i].pid,
                                protected[i].name,
                                task->signal->oom_score_adj,
                                protected[i].checkpoint_count,
                                protected[i].manual ?
                                "MANUAL" : "AUTO");
        }
        rcu_read_unlock();
    }
    spin_unlock_irqrestore(&protected_lock, flags);

    if (copy_to_user(buf, kbuf, len)) return -EFAULT;
    *pos = len;
    return len;
}

static ssize_t guard_write(struct file* file,
                            const char __user* buf,
                            size_t count, loff_t* pos) {
    char kbuf[16];
    pid_t pid;
    struct task_struct* task;

    if (count > sizeof(kbuf) - 1) return -EINVAL;
    if (copy_from_user(kbuf, buf, count)) return -EFAULT;
    kbuf[count] = '\0';
    if (kstrtoint(kbuf, 10, &pid) != 0) return -EINVAL;

    rcu_read_lock();
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (task)
        do_protect(task, 1);  // manual=1 bypasses heuristics
    else
        printk(KERN_WARNING "AI-OS: PID %d not found\n", pid);
    rcu_read_unlock();

    return count;
}

static const struct proc_ops guard_ops = {
    .proc_read  = guard_read,
    .proc_write = guard_write,
};

static const struct proc_ops checkpoint_ops = {
    .proc_read  = checkpoint_read,
    .proc_write = checkpoint_write,
};

static int __init ai_oom_guard_init(void) {
    proc_create(PROC_GUARD,      0666, NULL, &guard_ops);
    proc_create(PROC_CHECKPOINT, 0666, NULL, &checkpoint_ops);
    timer_setup(&scan_timer, timer_callback, 0);
    mod_timer(&scan_timer, jiffies + SCAN_INTERVAL);
    scan_and_protect();
    printk(KERN_INFO "AI-OS: OOM Guard v%s loaded\n",
           AIOS_VERSION);
    printk(KERN_INFO "AI-OS: RAM>%dMB + runtime>%ds\n",
           MIN_RAM_MB, MIN_RUNTIME_SECS);
    return 0;
}

static void __exit ai_oom_guard_exit(void) {
    timer_delete_sync(&scan_timer);
    remove_proc_entry(PROC_GUARD,      NULL);
    remove_proc_entry(PROC_CHECKPOINT, NULL);
    printk(KERN_INFO "AI-OS: OOM Guard unloaded\n");
}

module_init(ai_oom_guard_init);
module_exit(ai_oom_guard_exit);