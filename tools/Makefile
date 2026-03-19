CC     = gcc
CFLAGS = -Wall -Wextra -g

all: ai_monitor memory_hog training_sim oom_logger \
     auto_protect dashboard benchmark

ai_monitor: main.c proc_monitor.c oom_guard.c
	$(CC) $(CFLAGS) -o ai_monitor main.c proc_monitor.c oom_guard.c

memory_hog: memory_hog.c
	$(CC) $(CFLAGS) -o memory_hog memory_hog.c

training_sim: training_sim.c
	$(CC) $(CFLAGS) -o training_sim training_sim.c

oom_logger: oom_logger.c
	$(CC) $(CFLAGS) -o oom_logger oom_logger.c

auto_protect: auto_protect.c
	$(CC) $(CFLAGS) -o auto_protect auto_protect.c

dashboard: dashboard.c
	$(CC) $(CFLAGS) -o dashboard dashboard.c

benchmark: benchmark.c
	$(CC) $(CFLAGS) -o benchmark benchmark.c

clean:
	rm -f ai_monitor memory_hog training_sim oom_logger \
	      auto_protect dashboard benchmark