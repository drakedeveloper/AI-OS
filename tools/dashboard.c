#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <dirent.h>

volatile int running = 1;
void handle_signal(int sig) { (void)sig; running = 0; }

#define REFRESH 2

typedef struct {
    int  pid;
    char name[256];
    long ram_kb;
    int  oom_score;
    int  oom_adj;
} Process;

// Read kernel module data
void read_kernel_memory(char* buf, size_t size) {
    FILE* f = fopen("/proc/ai_memory", "r");
    if (!f) {
        snprintf(buf, size, "kernel module not loaded\n");
        return;
    }
    size_t n = fread(buf, 1, size - 1, f);
    buf[n] = '\0';
    fclose(f);
}

void read_kernel_guard(char* buf, size_t size) {
    FILE* f = fopen("/proc/ai_oom_guard", "r");
    if (!f) {
        snprintf(buf, size, "kernel module not loaded\n");
        return;
    }
    size_t n = fread(buf, 1, size - 1, f);
    buf[n] = '\0';
    fclose(f);
}

// Read system memory
void read_meminfo(long* total, long* used) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return;
    char line[256];
    long available = 0;
    *total = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:",     9)  == 0)
            sscanf(line, "%*s %ld", total);
        if (strncmp(line, "MemAvailable:", 13) == 0)
            sscanf(line, "%*s %ld", &available);
    }
    fclose(f);
    *used = *total - available;
}

// Draw memory bar
void draw_bar(long used, long total, int width) {
    float pct    = (float)used / total;
    int   filled = (int)(pct * width);
    printf("[");
    for (int i = 0; i < width; i++)
        printf(i < filled ? "█" : "░");
    printf("] %.1f%%", pct * 100);
}

// Get top processes by RAM
int get_top_processes(Process* procs, int max) {
    DIR* d = opendir("/proc");
    if (!d) return 0;
    int count = 0;
    struct dirent* entry;

    while ((entry = readdir(d)) != NULL && count < max) {
        int pid = atoi(entry->d_name);
        if (pid <= 0) continue;

        Process p;
        p.pid = pid;

        // name
        char path[256];
        snprintf(path, sizeof(path), "/proc/%d/comm", pid);
        FILE* f = fopen(path, "r");
        if (!f) continue;
        fgets(p.name, sizeof(p.name), f);
        fclose(f);
        p.name[strcspn(p.name, "\n")] = 0;

        // ram
        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        f = fopen(path, "r");
        if (!f) continue;
        char line[256];
        p.ram_kb = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line, "%*s %ld", &p.ram_kb);
                break;
            }
        }
        fclose(f);
        if (p.ram_kb == 0) continue;

        // oom score
        snprintf(path, sizeof(path), "/proc/%d/oom_score", pid);
        f = fopen(path, "r");
        p.oom_score = 0;
        if (f) { fscanf(f, "%d", &p.oom_score); fclose(f); }

        // oom adj
        snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
        f = fopen(path, "r");
        p.oom_adj = 0;
        if (f) { fscanf(f, "%d", &p.oom_adj); fclose(f); }

        procs[count++] = p;
    }
    closedir(d);

    // Sort by RAM descending
    for (int i = 0; i < count - 1; i++)
        for (int j = i + 1; j < count; j++)
            if (procs[j].ram_kb > procs[i].ram_kb) {
                Process tmp = procs[i];
                procs[i]    = procs[j];
                procs[j]    = tmp;
            }
    return count;
}

void get_timestamp(char* buf, size_t size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(buf, size, "%H:%M:%S", t);
}

void render(void) {
    printf("\033[2J\033[H");

    char ts[32];
    get_timestamp(ts, sizeof(ts));

    // Header
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║            AI-OS UNIFIED DASHBOARD  [%s]           ║\n", ts);
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    // Memory bar
    long total = 0, used = 0;
    read_meminfo(&total, &used);
    printf("SYSTEM MEMORY  ");
    draw_bar(used, total, 35);
    printf("  %ld MB / %ld MB\n\n", used/1024, total/1024);

    // Kernel modules section
    printf("┌─── KERNEL MODULE: OOM GUARD ───────────────────────────────┐\n");
    char guard_buf[512];
    read_kernel_guard(guard_buf, sizeof(guard_buf));
    // print each line indented
    char* saveptr1;
    char* line = strtok_r(guard_buf, "\n", &saveptr1);
    while (line) {
        printf("│  %-58s│\n", line);
        line = strtok_r(NULL, "\n", &saveptr1);
    }
    printf("└────────────────────────────────────────────────────────────┘\n\n");

    printf("┌─── KERNEL MODULE: MEMORY MONITOR ──────────────────────────┐\n");
    char mem_buf[512];
    read_kernel_memory(mem_buf, sizeof(mem_buf));
    char* saveptr2;
    line = strtok_r(mem_buf, "\n", &saveptr2);
    while (line) {
        printf("│  %-58s│\n", line);
        line = strtok_r(NULL, "\n", &saveptr2);
    }
    printf("└────────────────────────────────────────────────────────────┘\n\n");

    // Top processes
    printf("┌─── TOP PROCESSES BY MEMORY ────────────────────────────────┐\n");
    printf("│  %-8s %-18s %-10s %-10s %-10s│\n",
           "PID", "NAME", "RAM(MB)", "OOM", "STATUS");
    printf("│  %-8s %-18s %-10s %-10s %-10s│\n",
           "---", "----", "-------", "---", "------");

    Process procs[256];
    int count = get_top_processes(procs, 256);
    for (int i = 0; i < count && i < 10; i++) {
        char* status =
            procs[i].oom_adj <= -900 ? "PROTECTED" :
            procs[i].oom_score > 800 ? "CRITICAL"  :
            procs[i].oom_score > 600 ? "HIGH"       :
                                       "OK";
        printf("│  %-8d %-18s %-10ld %-10d %-10s│\n",
               procs[i].pid,
               procs[i].name,
               procs[i].ram_kb / 1024,
               procs[i].oom_score,
               status);
    }
    printf("└────────────────────────────────────────────────────────────┘\n");
    printf("\nRefreshing every %ds — Ctrl+C to exit\n", REFRESH);
    fflush(stdout);
}

int main(void) {
    signal(SIGINT, handle_signal);
    while (running) {
        render();
        sleep(REFRESH);
    }
    printf("\nDashboard stopped.\n");
    return 0;
}