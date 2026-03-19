<div align="center">

# AI-OS

### Kernel-Level OOM Protection for AI Training Workloads on Linux

[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![Kernel](https://img.shields.io/badge/Kernel-6.x+-orange.svg)](https://kernel.org)
[![Python](https://img.shields.io/badge/Python-3.8+-blue.svg)](https://python.org)
[![Version](https://img.shields.io/badge/Version-1.0.0-green.svg)](VERSION)

**The Linux OOM killer destroys your 18-hour training job. AI-OS stops it — at the kernel level.**

</div>

---

## The Problem

```
09:00 AM  —  Start training a large model (400MB loaded into RAM)
03:00 AM  —  System runs low on memory
03:01 AM  —  Linux OOM killer scores your training job highest
03:01 AM  —  18 hours of work killed instantly
03:01 AM  —  No warning. No checkpoint. Gone.
```

Linux was designed before multi-hour AI training runs existed.
The OOM killer treats your training job identically to a 1-second shell script.
It just sees a process using lots of RAM — and kills it.

**AI-OS fixes this at the kernel level.**

---

## Results

| Metric | Without AI-OS | With AI-OS |
|--------|:-------------:|:----------:|
| OOM Score | 666 | 67 |
| OOM Score Adj | 0 | -500 |
| Kill Risk | CRITICAL | PROTECTED |
| OOM Score Reduction | baseline | **833 points** |
| Memory Allocation (200MB) | 194.50ms avg | 29.04ms avg |
| Performance Improvement | baseline | **6.7x faster** |
| Auto-detection time | none | **< 5 seconds** |
| OOM Kills during stress test | unprotected | **0** |
| Scheduler delay (eBPF) | invisible | **max 7.40ms measured** |

---

## How It Works

```
User Space                        Kernel Space
──────────────────────────        ──────────────────────────────
import aios                 <-->  ai_oom_guard.ko
aios.protect()                      Scans all processes every 5s
aios.checkpoint(n)                  Auto-detects AI workloads
                                    Sets oom_score_adj = -500
                                    Tracks checkpoints

cat /proc/ai_memory         <-->  memory_monitor.ko
dashboard                           Real-time pressure stats
                                    Per-CPU OOM kill counter

cat /proc/ai_gpu            <-->  gpu_watcher.ko
                                    VRAM monitoring (NVIDIA)
                                    CUDA OOM prevention

sudo python3 ebpf_monitor   <-->  eBPF tracepoints
                                    sched_wakeup + sched_switch
                                    Scheduler delay measurement
```

### Auto-Detection Heuristic

AI-OS automatically protects a process when ALL conditions are met:

| Condition | Threshold | Reason |
|-----------|-----------|--------|
| Process name | python, python3, jupyter, nvcc, training_sim | Targets AI runtimes |
| RAM usage | > 200MB | Real training, not a script import |
| Runtime | > 5 seconds | Past initialization phase |

Manual override always available — bypasses all heuristics instantly.

---

## Install

```bash
git clone https://github.com/YOUR_USERNAME/ai-os
cd ai-os
sudo bash install.sh
```

That's it. The installer:
- Builds all kernel modules
- Loads them immediately
- Configures auto-load on every boot
- Installs the Python library
- Starts the systemd service

**Verified on:** Linux 6.18.12, Kali Rolling, x86_64

---

## Usage

### Python — one line protection

```python
import aios

aios.protect()      # protect current process from OOM killer
aios.status()       # show protection status
aios.checkpoint(n)  # report checkpoint to kernel
```

### With PyTorch training

```python
import aios
import torch

# Protect at the start — before model loads
aios.protect()

model     = MyModel()
optimizer = torch.optim.Adam(model.parameters())

for epoch in range(100):
    train_one_epoch(model, optimizer)

    # Save checkpoint and report to kernel
    aios.auto_checkpoint(model, optimizer, epoch,
                         f"checkpoint_{epoch}.pt",
                         interval=5)
```

### Terminal commands

```bash
# Live dashboard
sudo ./tools/dashboard

# Check protected processes
cat /proc/ai_oom_guard

# Check memory pressure
cat /proc/ai_memory

# Check GPU VRAM (NVIDIA)
cat /proc/ai_gpu

# Manually protect a process
echo <pid> | sudo tee /proc/ai_oom_guard

# eBPF scheduler monitor
sudo python3 tools/ebpf_monitor.py

# Run benchmarks
sudo ./tools/benchmark

# Stress test
sudo bash tools/stress_test.sh
```

### Python library only
```bash
pip install aios-guard
```

---

## Project Structure

```
ai-os/
│
├── kernel/                     # Linux kernel modules (C)
│   ├── ai_oom_guard.c          # Core OOM protection module
│   ├── memory_monitor.c        # Memory pressure monitor
│   ├── gpu_watcher.c           # GPU VRAM watcher (NVIDIA)
│   └── Makefile
│
├── tools/                      # User space tools (C)
│   ├── dashboard.c             # Unified live dashboard
│   ├── benchmark.c             # Performance benchmarking
│   ├── training_sim.c          # AI training simulator
│   ├── memory_hog.c            # Memory pressure simulator
│   ├── oom_logger.c            # OOM event logger
│   ├── auto_protect.c          # User space daemon
│   ├── real_training.py        # Real PyTorch training example
│   ├── ebpf_monitor.py         # eBPF scheduler monitor
│   ├── stress_test.sh          # Automated stress testing
│   └── Makefile
│
├── python/                     # Python package
│   └── aios/
│       ├── __init__.py
│       ├── core.py             # protect, checkpoint, status
│       └── monitor.py          # memory and GPU monitoring
│
├── install.sh                  # One command installer
├── uninstall.sh                # Clean uninstaller
└── VERSION                     # Current version
```

---

## /proc Filesystem Interface

AI-OS exposes four new entries in `/proc`:

| Entry | Access | Description |
|-------|--------|-------------|
| `/proc/ai_oom_guard` | read/write | Protection status, manual protect |
| `/proc/ai_checkpoint` | read/write | Checkpoint tracking |
| `/proc/ai_memory` | read | Memory pressure statistics |
| `/proc/ai_gpu` | read | GPU VRAM usage (NVIDIA) |

---

## Kernel APIs Used

```c
for_each_process()              // iterate all running processes
pid_task() / find_vpid()        // PID to task_struct lookup
task->signal->oom_score_adj     // direct OOM score modification
proc_create()                   // /proc filesystem extension
timer_setup() / mod_timer()     // periodic scanning
si_meminfo()                    // system memory statistics
per_cpu(vm_event_states, cpu)   // per-CPU OOM kill counter
DEFINE_SPINLOCK()               // kernel concurrency protection
rcu_read_lock()                 // RCU critical sections
```

---

## Requirements

| Requirement | Version |
|-------------|---------|
| Linux kernel | 6.x+ |
| gcc | Any recent |
| make | Any recent |
| Python | 3.8+ |
| Root access | Required for kernel modules |
| PyTorch | Optional (for real_training.py) |
| BCC tools | Optional (for ebpf_monitor.py) |

---

## Uninstall

```bash
sudo bash uninstall.sh
```

Removes all kernel modules, systemd service, proc entries, and Python package cleanly.

---

## Limitations

- Requires root for kernel module loading
- Detection by process name — may not catch all AI workloads
- GPU support requires real NVIDIA hardware (VM shows placeholder)
- Tested on x86_64 — ARM/edge port planned

---

## License

GPL v2

---

<div align="center">

**Built by Hamza Trabelsi**

*The kernel should know what matters. Now it does.*

</div>