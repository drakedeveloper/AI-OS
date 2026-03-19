#!/bin/bash
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: Run with sudo"
    exit 1
fi

echo "=== AI-OS Uninstaller ==="

sudo rmmod ai_oom_guard   2>/dev/null || true
sudo rmmod memory_monitor 2>/dev/null || true
sudo rmmod gpu_watcher    2>/dev/null || true

sudo rm -f /etc/modules-load.d/ai-os.conf
sudo rm -f /etc/systemd/system/ai-os.service
sudo rm -f /usr/local/bin/ai-os-guard
sudo rm -f /lib/modules/$(uname -r)/extra/ai_oom_guard.ko
sudo rm -f /lib/modules/$(uname -r)/extra/memory_monitor.ko
sudo rm -f /lib/modules/$(uname -r)/extra/gpu_watcher.ko

sudo depmod -a
sudo systemctl daemon-reload

pip uninstall aios -y 2>/dev/null || true

echo "AI-OS uninstalled successfully."