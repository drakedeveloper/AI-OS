#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "proc_monitor.h"
#include "oom_guard.h"

volatile int running = 1;
void handle_signal(int sig) { (void)sig; running = 0; }

int main(int argc, char* argv[]) {

    if (argc == 3 && strcmp(argv[1], "protect") == 0) {
        return protect_process(atoi(argv[2]));
    }
    if (argc == 3 && strcmp(argv[1], "unprotect") == 0) {
        return unprotect_process(atoi(argv[2]));
    }

    signal(SIGINT, handle_signal);
    while (running) {
        scan_and_print();
        sleep(REFRESH_RATE);
    }
    printf("\nMonitor stopped.\n");
    return 0;
}