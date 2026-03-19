#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

volatile int running = 1;
void handle_signal(int sig) { (void)sig; running = 0; }

int main(int argc, char* argv[]) {
    // Default: eat 500MB — change with argument
    int mb = argc > 1 ? atoi(argv[1]) : 500;

    printf("=== MEMORY HOG ===\n");
    printf("PID: %d\n", getpid());
    printf("Allocating %d MB...\n", mb);

    signal(SIGINT, handle_signal);

    size_t bytes = (size_t)mb * 1024 * 1024;
    char* memory = malloc(bytes);

    if (!memory) {
        printf("malloc failed — not enough memory\n");
        return 1;
    }

    // Actually touch the memory — forces OS to allocate real pages
    // malloc alone is lazy — OS doesn't give real RAM until you use it
    printf("Touching memory pages...\n");
    for (size_t i = 0; i < bytes; i += 4096) {
        memory[i] = 1;
    }

    printf("Holding %d MB of RAM\n", mb);
    printf("Check your monitor — find this PID and protect it\n");
    printf("Press Ctrl+C to release memory\n\n");

    // Keep running — hold the memory
    while (running) {
        sleep(1);
    }

    printf("\nReleasing memory...\n");
    free(memory);
    return 0;
}