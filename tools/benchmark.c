#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define TEST_MB     200
#define ITERATIONS  5

double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int read_oom_score(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/oom_score", pid);
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    int score = 0;
    fscanf(f, "%d", &score);
    fclose(f);
    return score;
}

int read_oom_adj(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    int adj = 0;
    fscanf(f, "%d", &adj);
    fclose(f);
    return adj;
}

void protect_self(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
    FILE* f = fopen(path, "w");
    if (!f) { printf("Need sudo to protect\n"); return; }
    fprintf(f, "-900");
    fclose(f);
}

void unprotect_self(int pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "0");
    fclose(f);
}

// Allocate and touch TEST_MB — measure time
double benchmark_alloc(void) {
    size_t  bytes = (size_t)TEST_MB * 1024 * 1024;
    double  start = get_time_ms();
    char*   mem   = malloc(bytes);
    if (!mem) { printf("malloc failed\n"); return -1; }

    for (size_t i = 0; i < bytes; i += 4096)
        mem[i] = (char)(i % 256);

    double elapsed = get_time_ms() - start;
    free(mem);
    return elapsed;
}

int main(void) {
    int pid = getpid();

    printf("╔══════════════════════════════════════════╗\n");
    printf("║         AI-OS BENCHMARK TOOL             ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");
    printf("PID: %d\n", pid);
    printf("Test: allocate + touch %d MB x %d iterations\n\n",
           TEST_MB, ITERATIONS);

    // Phase 1 — without protection
    printf("PHASE 1: Without OOM protection\n");
    printf("─────────────────────────────────\n");
    unprotect_self(pid);
    printf("OOM score:     %d\n", read_oom_score(pid));
    printf("OOM adj:       %d\n\n", read_oom_adj(pid));

    double total1 = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        double t = benchmark_alloc();
        printf("  Run %d: %.2f ms\n", i + 1, t);
        total1 += t;
    }
    printf("Average: %.2f ms\n\n", total1 / ITERATIONS);

    // Phase 2 — with protection
    printf("PHASE 2: With OOM protection\n");
    printf("─────────────────────────────────\n");
    protect_self(pid);
    printf("OOM score:     %d\n", read_oom_score(pid));
    printf("OOM adj:       %d\n\n", read_oom_adj(pid));

    double total2 = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        double t = benchmark_alloc();
        printf("  Run %d: %.2f ms\n", i + 1, t);
        total2 += t;
    }
    printf("Average: %.2f ms\n\n", total2 / ITERATIONS);

    // Results
    printf("╔══════════════════════════════════════════╗\n");
    printf("║              RESULTS                     ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Without protection: %8.2f ms avg      ║\n",
           total1 / ITERATIONS);
    printf("║  With protection:    %8.2f ms avg      ║\n",
           total2 / ITERATIONS);
    printf("║                                          ║\n");
    printf("║  OOM score reduced by: %d points        ║\n",
           read_oom_score(pid) == -1 ? 0 :
           900 - read_oom_score(pid));
    printf("╚══════════════════════════════════════════╝\n");

    unprotect_self(pid);
    return 0;
}