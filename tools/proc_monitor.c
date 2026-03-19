#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "proc_monitor.h"

long read_proc_status(int pid, const char* key) {
    char path[256], line[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, strlen(key)) == 0) {
            fclose(f);
            long value;
            sscanf(line, "%*s %ld", &value);
            return value;
        }
    }
    fclose(f);
    return -1;
}

void read_proc_name(int pid, char* name, size_t size) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE* f = fopen(path, "r");
    if (!f) { strncpy(name, "unknown", size); return; }
    fgets(name, size, f);
    fclose(f);
    name[strcspn(name, "\n")] = 0;
}

int read_oom_adj(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    int val = 0;
    fscanf(f, "%d", &val);
    fclose(f);
    return val;
}

void read_memory_info(long* total, long* available, long* used) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return;
    char line[256];
    *total = *available = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:",     9)  == 0) sscanf(line, "%*s %ld", total);
        if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line, "%*s %ld", available);
    }
    fclose(f);
    *used = *total - *available;
}

void draw_memory_bar(long used, long total) {
    int bar_width = 40;
    float pct = (float)used / total;
    int filled = (int)(pct * bar_width);
    printf("RAM [");
    for (int i = 0; i < bar_width; i++)
        printf(i < filled ? "█" : "░");
    printf("] %ld MB / %ld MB (%.1f%%)\n",
           used/1024, total/1024, pct*100);
}

static int compare_oom(const void* a, const void* b) {
    return ((Process*)b)->oom_score - ((Process*)a)->oom_score;
}

int scan_processes(Process* processes, int max) {
    int count = 0;
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return 0;
    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != NULL && count < max) {
        int pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        Process p;
        p.pid = pid;
        read_proc_name(pid, p.name, sizeof(p.name));
        p.ram_kb = read_proc_status(pid, "VmRSS");
        if (p.ram_kb < 0) continue;
        char path[256];
        snprintf(path, sizeof(path), "/proc/%d/oom_score", pid);
        FILE* f = fopen(path, "r");
        p.oom_score = -1;
        if (f) { fscanf(f, "%d", &p.oom_score); fclose(f); }
        p.oom_adj   = read_oom_adj(pid);
        p.protected = (p.oom_adj <= -900) ? 1 : 0;
        processes[count++] = p;
    }
    closedir(proc_dir);
    qsort(processes, count, sizeof(Process), compare_oom);
    return count;
}

void print_process_table(Process* processes, int count) {
    printf("%-8s %-20s %-12s %-12s %-15s\n",
           "PID","NAME","RAM(KB)","OOM_SCORE","STATUS");
    printf("%-8s %-20s %-12s %-12s %-15s\n",
           "---","----","-------","---------","------");
    for (int i = 0; i < count && i < 15; i++) {
        char* status;
        if      (processes[i].protected)        status = "PROTECTED";
        else if (processes[i].oom_score > 800)  status = "CRITICAL";
        else if (processes[i].oom_score > 600)  status = "HIGH";
        else if (processes[i].oom_score > 400)  status = "MEDIUM";
        else                                     status = "LOW";
        printf("%-8d %-20s %-12ld %-12d %-15s\n",
               processes[i].pid, processes[i].name,
               processes[i].ram_kb, processes[i].oom_score, status);
    }
}

void scan_and_print() {
    Process processes[MAX_PROCESSES];
    int count = scan_processes(processes, MAX_PROCESSES);

    printf("\033[2J\033[H");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║         AI-OS PROCESS MONITOR  (Ctrl+C to exit)     ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    long total = 0, available = 0, used = 0;
    read_memory_info(&total, &available, &used);
    draw_memory_bar(used, total);

    float mem_pct = (float)used / total * 100;
    if (mem_pct > 80)
        printf("WARNING: Memory at %.1f%% — OOM killer may activate!\n\n", mem_pct);
    else
        printf("Memory OK\n\n");

    print_process_table(processes, count);
    printf("\nTotal: %d processes\n", count);
    fflush(stdout);
}