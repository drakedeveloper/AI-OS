#!/usr/bin/env python3
# eBPF-based training monitor
# Watches scheduler and memory events for AI processes

try:
    from bcc import BPF
except ImportError:
    print("Installing BCC...")
    os.system("sudo apt install -y python3-bpfcc")
    from bcc import BPF

import os
import time
import signal

running = True
def handle_signal(sig, frame):
    global running
    running = False

signal.signal(signal.SIGINT, handle_signal)

# eBPF program — runs inside kernel
BPF_PROGRAM = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

// Shared map — kernel writes, Python reads
BPF_HASH(oom_score_map, u32, u32);
BPF_HASH(sched_delay,   u32, u64);
BPF_PERF_OUTPUT(events);

struct event_t {
    u32  pid;
    char comm[16];
    u64  delay_ns;
    u32  event_type;  // 1=sched 2=oom
};

// Track scheduler wakeup delays
TRACEPOINT_PROBE(sched, sched_wakeup) {
    u32 pid = args->pid;
    u64 ts  = bpf_ktime_get_ns();
    sched_delay.update(&pid, &ts);
    return 0;
}

// Measure how long process waited to run
TRACEPOINT_PROBE(sched, sched_switch) {
    u32  pid = args->next_pid;
    u64* ts  = sched_delay.lookup(&pid);
    if (!ts) return 0;

    u64 delay = bpf_ktime_get_ns() - *ts;
    sched_delay.delete(&pid);

    // Only report significant delays (>1ms)
    if (delay > 1000000) {
        struct event_t e = {};
        e.pid        = pid;
        e.delay_ns   = delay;
        e.event_type = 1;
        bpf_get_current_comm(&e.comm, sizeof(e.comm));
        events.perf_submit(args, &e, sizeof(e));
    }
    return 0;
}
"""

print("=== AI-OS eBPF TRAINING MONITOR ===")
print("Watching scheduler delays for AI processes...")
print("Press Ctrl+C to stop\n")

b = BPF(text=BPF_PROGRAM)

ai_names = {b"python", b"python3", b"training_sim"}

def handle_event(cpu, data, size):
    event = b["events"].event(data)
    name  = event.comm.decode('utf-8', errors='replace').strip('\x00')

    if name.encode() in ai_names or any(
            n.decode() in name for n in ai_names):
        delay_ms = event.delay_ns / 1e6
        print(f"SCHED DELAY | PID:{event.pid:6d} "
              f"NAME:{name:16s} "
              f"DELAY:{delay_ms:.2f}ms")

b["events"].open_perf_buffer(handle_event)

print("Monitoring... (Ctrl+C to stop)\n")
while running:
    try:
        b.perf_buffer_poll(timeout=100)
    except KeyboardInterrupt:
        break

print("\nMonitor stopped.")