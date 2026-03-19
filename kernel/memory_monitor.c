#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/vmstat.h>
#include <linux/cpumask.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Trabelsi");
MODULE_DESCRIPTION("AI-OS: Memory pressure monitor v1.0");

#define PROC_FILENAME  "ai_memory"
#define SCAN_INTERVAL  (2 * HZ)

static unsigned long total_mb  = 0;
static unsigned long free_mb   = 0;
static unsigned long used_mb   = 0;
static unsigned long oom_kills = 0;
static int           pressure  = 0;
static struct timer_list mem_timer;

static unsigned long get_oom_kills(void) {
    unsigned long total = 0;
    int cpu;
    for_each_possible_cpu(cpu)
        total += per_cpu(vm_event_states, cpu).event[OOM_KILL];
    return total;
}

static void update_stats(void) {
    struct sysinfo si;
    si_meminfo(&si);
    total_mb = (si.totalram  * si.mem_unit) >> 20;
    free_mb  = (si.freeram   * si.mem_unit) >> 20;
    used_mb  = total_mb - free_mb;
    if (total_mb > 0)
        pressure = (int)((used_mb * 100) / total_mb);
    oom_kills = get_oom_kills();
    if (pressure > 85)
        printk(KERN_WARNING
               "AI-OS: Memory pressure CRITICAL: %d%%\n",
               pressure);
}

static void timer_callback(struct timer_list* t) {
    update_stats();
    mod_timer(&mem_timer, jiffies + SCAN_INTERVAL);
}

static ssize_t proc_read(struct file* file,
                          char __user* buf,
                          size_t count, loff_t* pos) {
    char kbuf[512];
    int  len = 0;

    if (*pos > 0) return 0;
    update_stats();

    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "AI-OS Memory Monitor\n");
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "====================\n");
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "Total:     %lu MB\n", total_mb);
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "Used:      %lu MB\n", used_mb);
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "Free:      %lu MB\n", free_mb);
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "Pressure:  %d%%\n",   pressure);
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "OOM Kills: %lu\n",    oom_kills);
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "Status:    %s\n",
                    pressure > 85 ? "CRITICAL" :
                    pressure > 70 ? "WARNING"  : "OK");

    if (copy_to_user(buf, kbuf, len)) return -EFAULT;
    *pos = len;
    return len;
}

static const struct proc_ops mem_proc_ops = {
    .proc_read = proc_read,
};

static int __init memory_monitor_init(void) {
    proc_create(PROC_FILENAME, 0444, NULL, &mem_proc_ops);
    timer_setup(&mem_timer, timer_callback, 0);
    mod_timer(&mem_timer, jiffies + SCAN_INTERVAL);
    update_stats();
    printk(KERN_INFO "AI-OS: Memory Monitor loaded\n");
    return 0;
}

static void __exit memory_monitor_exit(void) {
    timer_delete_sync(&mem_timer);
    remove_proc_entry(PROC_FILENAME, NULL);
    printk(KERN_INFO "AI-OS: Memory Monitor unloaded\n");
}

module_init(memory_monitor_init);
module_exit(memory_monitor_exit);