#include <stdio.h>
#include "oom_guard.h"

int protect_process(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
    FILE* f = fopen(path, "w");
    if (!f) { printf("ERROR: Need sudo for PID %d\n", pid); return -1; }
    fprintf(f, "-900");
    fclose(f);
    printf("PID %d is now PROTECTED\n", pid);
    return 0;
}

int unprotect_process(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
    FILE* f = fopen(path, "w");
    if (!f) { printf("ERROR: Need sudo for PID %d\n", pid); return -1; }
    fprintf(f, "0");
    fclose(f);
    printf("PID %d protection removed\n", pid);
    return 0;
}