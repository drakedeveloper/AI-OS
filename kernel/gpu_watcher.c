#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Trabelsi");
MODULE_DESCRIPTION("AI-OS: GPU Memory Watcher");

#define PROC_FILENAME  "ai_gpu"
#define SCAN_INTERVAL  (3 * HZ)
#define WARN_PERCENT   80

// GPU memory info — read from sysfs if NVIDIA present
// Falls back to mock data if no GPU
typedef struct {
    unsigned long vram_total_mb;
    unsigned long vram_used_mb;
    unsigned long vram_free_mb;
    int           percent;
    int           has_gpu;
    char          gpu_name[64];
} GpuInfo;

static GpuInfo gpu_info;
static struct timer_list gpu_timer;

// Try to read NVIDIA GPU memory from sysfs
static int read_nvidia_memory(GpuInfo* info) {
    // NVIDIA exposes memory via:
    // /sys/class/drm/card0/device/mem_info_vram_total
    // /sys/class/drm/card0/device/mem_info_vram_used
    // This works on real NVIDIA hardware
    // On VM — returns 0 (no GPU)

    struct file* f;
    char buf[64];
    loff_t pos = 0;
    ssize_t n;

    f = filp_open("/sys/class/drm/card0/device/mem_info_vram_total",
                  O_RDONLY, 0);
    if (IS_ERR(f)) return 0;

    n = kernel_read(f, buf, sizeof(buf) - 1, &pos);
    filp_close(f, NULL);

    if (n <= 0) return 0;
    buf[n] = '\0';

    unsigned long total_bytes;
    if (kstrtoul(buf, 10, &total_bytes) != 0) return 0;
    info->vram_total_mb = total_bytes >> 20;

    // Read used
    pos = 0;
    f = filp_open("/sys/class/drm/card0/device/mem_info_vram_used",
                  O_RDONLY, 0);
    if (IS_ERR(f)) return 0;

    n = kernel_read(f, buf, sizeof(buf) - 1, &pos);
    filp_close(f, NULL);
    if (n <= 0) return 0;
    buf[n] = '\0';

    unsigned long used_bytes;
    if (kstrtoul(buf, 10, &used_bytes) != 0) return 0;
    info->vram_used_mb = used_bytes >> 20;
    info->vram_free_mb = info->vram_total_mb - info->vram_used_mb;

    if (info->vram_total_mb > 0)
        info->percent = (int)((info->vram_used_mb * 100)
                              / info->vram_total_mb);
    return 1;
}

static void update_gpu_info(void) {
    if (!read_nvidia_memory(&gpu_info)) {
        // No real GPU — show VM placeholder
        gpu_info.has_gpu      = 0;
        gpu_info.vram_total_mb = 0;
        gpu_info.vram_used_mb  = 0;
        gpu_info.vram_free_mb  = 0;
        gpu_info.percent       = 0;
        strncpy(gpu_info.gpu_name,
                "No GPU detected (VM environment)",
                sizeof(gpu_info.gpu_name) - 1);
    } else {
        gpu_info.has_gpu = 1;
        strncpy(gpu_info.gpu_name, "NVIDIA GPU",
                sizeof(gpu_info.gpu_name) - 1);

        if (gpu_info.percent >= WARN_PERCENT)
            printk(KERN_WARNING
                   "AI-OS: GPU VRAM at %d%% — "
                   "risk of CUDA OOM\n",
                   gpu_info.percent);
    }
}

static void timer_callback(struct timer_list* t) {
    update_gpu_info();
    mod_timer(&gpu_timer, jiffies + SCAN_INTERVAL);
}

static ssize_t proc_read(struct file* file,
                          char __user* buf,
                          size_t count, loff_t* pos) {
    char kbuf[512];
    int  len = 0;

    if (*pos > 0) return 0;

    update_gpu_info();

    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "AI-OS GPU Watcher\n");
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "=================\n");
    len += snprintf(kbuf + len, sizeof(kbuf) - len,
                    "GPU:      %s\n", gpu_info.gpu_name);

    if (gpu_info.has_gpu) {
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "VRAM Total: %lu MB\n",
                        gpu_info.vram_total_mb);
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "VRAM Used:  %lu MB\n",
                        gpu_info.vram_used_mb);
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "VRAM Free:  %lu MB\n",
                        gpu_info.vram_free_mb);
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "Pressure:   %d%%\n",
                        gpu_info.percent);
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "Status:     %s\n",
                        gpu_info.percent >= WARN_PERCENT ?
                        "WARNING" : "OK");
    } else {
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "Status:   No GPU present\n");
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "Note:     Module ready for "
                        "NVIDIA hardware\n");
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "Sysfs:    /sys/class/drm/card0/"
                        "device/mem_info_vram_*\n");
    }

    if (copy_to_user(buf, kbuf, len)) return -EFAULT;
    *pos = len;
    return len;
}

static const struct proc_ops gpu_proc_ops = {
    .proc_read = proc_read,
};

static int __init gpu_watcher_init(void) {
    proc_create(PROC_FILENAME, 0444, NULL, &gpu_proc_ops);
    timer_setup(&gpu_timer, timer_callback, 0);
    mod_timer(&gpu_timer, jiffies + SCAN_INTERVAL);
    update_gpu_info();
    printk(KERN_INFO "AI-OS: GPU Watcher loaded\n");
    printk(KERN_INFO "AI-OS: GPU detected: %s\n",
           gpu_info.has_gpu ? "YES" : "NO (VM)");
    return 0;
}

static void __exit gpu_watcher_exit(void) {
    timer_delete_sync(&gpu_timer);
    remove_proc_entry(PROC_FILENAME, NULL);
    printk(KERN_INFO "AI-OS: GPU Watcher unloaded\n");
}

module_init(gpu_watcher_init);
module_exit(gpu_watcher_exit);