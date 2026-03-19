#!/bin/bash
set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

BASE=/home/kali/ai-os

echo "================================"
echo "   AI-OS Installation v1.0"
echo "================================"

# 1. Build user tools
echo -e "\n${GREEN}[1/5] Building user space tools...${NC}"
cd $BASE/tools
make clean && make all

# 2. Build kernel modules
echo -e "\n${GREEN}[2/5] Building kernel modules...${NC}"
cd $BASE/kernel
make clean && make all

# 3. Install kernel modules
echo -e "\n${GREEN}[3/5] Installing kernel modules...${NC}"
sudo mkdir -p /lib/modules/$(uname -r)/extra/
sudo cp $BASE/kernel/ai_oom_guard.ko \
        /lib/modules/$(uname -r)/extra/
sudo cp $BASE/kernel/memory_monitor.ko \
        /lib/modules/$(uname -r)/extra/
sudo cp $BASE/kernel/gpu_watcher.ko \
        /lib/modules/$(uname -r)/extra/
sudo depmod -a

# 4. Load kernel modules
echo -e "\n${GREEN}[4/5] Loading kernel modules...${NC}"
sudo rmmod ai_oom_guard   2>/dev/null || true
sudo rmmod memory_monitor 2>/dev/null || true
sudo rmmod gpu_watcher    2>/dev/null || true
sudo insmod $BASE/kernel/ai_oom_guard.ko
sudo insmod $BASE/kernel/memory_monitor.ko
sudo insmod $BASE/kernel/gpu_watcher.ko

# Auto-load on boot
echo "ai_oom_guard"   | sudo tee    /etc/modules-load.d/ai-os.conf
echo "memory_monitor" | sudo tee -a /etc/modules-load.d/ai-os.conf
echo "gpu_watcher"    | sudo tee -a /etc/modules-load.d/ai-os.conf

# 5. Install Python package
echo -e "\n${GREEN}[5/5] Installing Python package...${NC}"
cd $BASE/python
pip install -e . --break-system-packages

# systemd service
sudo cp $BASE/tools/auto_protect /usr/local/bin/ai-os-guard
sudo tee /etc/systemd/system/ai-os.service > /dev/null <<EOF
[Unit]
Description=AI-OS OOM Guard Service
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/ai-os-guard
Restart=always
RestartSec=5
User=root

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable ai-os
sudo systemctl start ai-os

echo -e "\n${GREEN}================================${NC}"
echo -e "${GREEN}   AI-OS Installation Complete${NC}"
echo -e "${GREEN}================================${NC}"
echo ""
echo "Kernel modules loaded:"
lsmod | grep -E "ai_oom|memory_monitor|gpu_watcher"
echo ""
echo "Proc entries:"
ls /proc/ai_*
echo ""
echo "Service status:"
systemctl is-active ai-os
echo ""
echo "Test it:"
echo "  python3 -c \"import aios; aios.protect(); aios.status()\""
echo "  cat /proc/ai_oom_guard"
echo "  cat /proc/ai_memory"
echo "  cat /proc/ai_gpu"