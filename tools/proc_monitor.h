#ifndef PROC_MONITOR_H
#define PROC_MONITOR_H

#define MAX_PROCESSES 1024
#define REFRESH_RATE  1

typedef struct {
    int pid;
    char name[256];
    long ram_kb;
    int oom_score;
    int oom_adj;
    int protected;
} Process;

// Function declarations
long read_proc_status(int pid, const char* key);
void read_proc_name(int pid, char* name, size_t size);
int  read_oom_adj(int pid);
void read_memory_info(long* total, long* available, long* used);
void draw_memory_bar(long used, long total);
int  scan_processes(Process* processes, int max);
void print_process_table(Process* processes, int count);
void scan_and_print();

#endif