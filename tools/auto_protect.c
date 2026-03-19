#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>

volatile int running = 1;
void handle_signal(int sig) { (void)sig; running = 0; }

// Names that indicate AI training processes
const char* AI_PROCESS_NAMES[] = {
    "python",
    "python3",
    "training_sim",
    "jupyter",
    "torch",
    NULL  // sentinel — marks end of list
};

int is_ai_process(const char* name) {
    for (int i = 0; AI_PROCESS_NAMES[i] != NULL; i++) {
        if (strncmp(name, AI_PROCESS_NAMES[i],
                    strlen(AI_PROCESS_NAMES[i])) == 0)
            return 1;
    }
    return 0;
}

int get_oom_adj(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    int val = 0;
    fscanf(f, "%d", &val);
    fclose(f);
    return val;
}

int protect_pid(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "-900");
    fclose(f);
    return 0;
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

void scan_and_protect() {
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return;

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        int pid = atoi(entry->d_name);
        if (pid <= 0) continue;

        char name[256];
        read_proc_name(pid, name, sizeof(name));

        if (is_ai_process(name)) {
            int adj = get_oom_adj(pid);
            if (adj > -900) {
                if (protect_pid(pid) == 0)
                    printf("AUTO-PROTECTED: PID %d (%s)\n", pid, name);
            }
        }
    }
    closedir(proc_dir);
}

int main() {
    signal(SIGINT, handle_signal);

    printf("=== AUTO PROTECTOR DAEMON ===\n");
    printf("Watching for AI processes...\n");
    printf("Protected names: ");
    for (int i = 0; AI_PROCESS_NAMES[i] != NULL; i++)
        printf("%s ", AI_PROCESS_NAMES[i]);
    printf("\n\nPress Ctrl+C to stop\n\n");

    while (running) {
        scan_and_protect();
        sleep(2);
    }

    printf("\nDaemon stopped.\n");
    return 0;
}