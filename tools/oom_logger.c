#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

volatile int running = 1;
void handle_signal(int sig) { (void)sig; running = 0; }

#define LOG_FILE     "oom_events.log"
#define WARN_PERCENT 80

void get_timestamp(char* buf, size_t size) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", t);
}

typedef struct {
    long total;
    long available;
    long used;
    float percent;
} MemInfo;

MemInfo read_mem() {
    MemInfo m = {0};
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return m;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:",     9)  == 0) sscanf(line, "%*s %ld", &m.total);
        if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line, "%*s %ld", &m.available);
    }
    fclose(f);
    m.used    = m.total - m.available;
    m.percent = (float)m.used / m.total * 100.0f;
    return m;
}

int was_oom_killed(int last_count) {
    // Check /proc/vmstat for oom_kill counter
    FILE* f = fopen("/proc/vmstat", "r");
    if (!f) return 0;
    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "oom_kill", 8) == 0) {
            sscanf(line, "%*s %d", &count);
            break;
        }
    }
    fclose(f);
    return count - last_count;
}

int main() {
    signal(SIGINT, handle_signal);

    FILE* log = fopen(LOG_FILE, "a");
    if (!log) { perror("Cannot open log"); return 1; }

    printf("=== OOM EVENT LOGGER ===\n");
    printf("Logging to: %s\n", LOG_FILE);
    printf("Warning threshold: %d%%\n\n", WARN_PERCENT);

    char ts[64];
    get_timestamp(ts, sizeof(ts));
    fprintf(log, "\n[%s] Logger started\n", ts);
    fflush(log);

    int last_oom_count = 0;

    // Get initial oom_kill count
    FILE* f = fopen("/proc/vmstat", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "oom_kill", 8) == 0) {
                sscanf(line, "%*s %d", &last_oom_count);
                break;
            }
        }
        fclose(f);
    }

    int warned = 0;

    while (running) {
        MemInfo m = read_mem();

        // Check for new OOM kills
        int new_kills = was_oom_killed(last_oom_count);
        if (new_kills > 0) {
            last_oom_count += new_kills;
            get_timestamp(ts, sizeof(ts));
            printf("[%s] OOM KILL DETECTED — %d process(es) killed!\n",
                   ts, new_kills);
            fprintf(log, "[%s] OOM_KILL: %d processes killed. "
                    "Memory: %.1f%%\n", ts, new_kills, m.percent);
            fflush(log);
        }

        // Warn when memory is high
        if (m.percent >= WARN_PERCENT && !warned) {
            get_timestamp(ts, sizeof(ts));
            printf("[%s] WARNING: Memory at %.1f%%\n", ts, m.percent);
            fprintf(log, "[%s] WARNING: Memory at %.1f%% "
                    "(%ld MB / %ld MB)\n",
                    ts, m.percent, m.used/1024, m.total/1024);
            fflush(log);
            warned = 1;
        }

        if (m.percent < WARN_PERCENT) warned = 0;

        printf("\rMemory: %.1f%% (%ld MB / %ld MB) | "
               "OOM kills logged: %d    ",
               m.percent, m.used/1024, m.total/1024,
               last_oom_count);
        fflush(stdout);

        sleep(1);
    }

    get_timestamp(ts, sizeof(ts));
    fprintf(log, "[%s] Logger stopped\n", ts);
    fclose(log);
    printf("\n\nLog saved to %s\n", LOG_FILE);
    return 0;
}