#!/usr/bin/env python3
"""
AI-OS eBPF Training Monitor
Tracks scheduler delays and memory events for AI processes
"""

import os
import sys
import time
import signal

try:
    from bcc import BPF
except ImportError:
    print("Install: sudo apt install python3-bpfcc")
    sys.exit(1)

AI_NAMES = {b"python3", b"python", b"training_sim"}

BPF_PROGRAM = r"""
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>
#include <linux/mm.h>

struct event_t {
    u32  pid;
    u32  event_type;  // 1=sched_delay 2=oom_score
    u64  value;
    char comm[16];
};

BPF_PERF_OUTPUT(events);
BPF_HASH(wakeup_ts, u32, u64);

// Track when process wakes up
TRACEPOINT_PROBE(sched, sched_wakeup) {
    u32 pid = args->pid;
    u64 ts  = bpf_ktime_get_ns();
    wakeup_ts.update(&pid, &ts);
    return 0;
}

// Measure delay from wakeup to actually running
TRACEPOINT_PROBE(sched, sched_switch) {
    u32  pid = args->next_pid;
    u64* ts  = wakeup_ts.lookup(&pid);
    if (!ts) return 0;

    u64 delay = bpf_ktime_get_ns() - *ts;
    wakeup_ts.delete(&pid);

    // Only report delays > 1ms
    if (delay < 1000000) return 0;

    struct event_t e = {};
    e.pid        = pid;
    e.event_type = 1;
    e.value      = delay;
    bpf_get_current_comm(&e.comm, sizeof(e.comm));
    events.perf_submit(args, &e, sizeof(e));
    return 0;
}
"""

running = True
def stop(sig, frame):
    global running
    running = False

signal.signal(signal.SIGINT, stop)

print("=== AI-OS eBPF MONITOR ===")
print("Tracking scheduler delays for AI processes")
print("Press Ctrl+C to stop\n")

b         = BPF(text=BPF_PROGRAM)
delays    = []
max_delay = 0
count     = 0

def handle_event(cpu, data, size):
    global max_delay, count
    e     = b["events"].event(data)
    name  = e.comm.decode('utf-8', errors='replace').strip('\x00')
    bname = name.encode()

    if not any(bname.startswith(n) for n in AI_NAMES):
        return

    delay_ms = e.value / 1e6
    delays.append(delay_ms)
    count += 1

    if delay_ms > max_delay:
        max_delay = delay_ms

    print(f"SCHED_DELAY | PID:{e.pid:6d} "
          f"NAME:{name:16s} "
          f"DELAY:{delay_ms:8.2f}ms "
          f"MAX:{max_delay:.2f}ms "
          f"COUNT:{count}")

b["events"].open_perf_buffer(handle_event)

while running:
    try:
        b.perf_buffer_poll(timeout=100)
    except KeyboardInterrupt:
        break

if delays:
    avg = sum(delays) / len(delays)
    print(f"\n=== SUMMARY ===")
    print(f"Total delays:   {count}")
    print(f"Average delay:  {avg:.2f}ms")
    print(f"Max delay:      {max_delay:.2f}ms")
    print(f"\nPaper data:")
    print(f"Scheduler interference: avg {avg:.2f}ms, "
          f"max {max_delay:.2f}ms")
else:
    print("\nNo significant delays detected")