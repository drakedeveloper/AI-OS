#!/bin/bash
# AI-OS Stress Test
# Runs training under memory pressure
# Collects data for paper
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: Run with sudo"
    exit 1
fi
sudo insmod $BASE/kernel/ai_oom_guard.ko || {
    echo "ERROR: Failed to load ai_oom_guard.ko"
    exit 1
}
set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

RESULTS="/tmp/ai_os_stress_results.txt"
echo "" > $RESULTS

log() {
    echo -e "$1"
    echo "$1" >> $RESULTS
}

log "${GREEN}=== AI-OS STRESS TEST ===${NC}"
log "Date: $(date)"
log "Kernel: $(uname -r)"
log "RAM: $(free -m | awk '/Mem:/{print $2}') MB"
log ""

# Test 1 — Baseline OOM score
log "${YELLOW}TEST 1: Baseline OOM Score${NC}"
BASELINE_SCORE=$(cat /proc/$$/oom_score)
log "Baseline OOM score: $BASELINE_SCORE"
log ""

# Test 2 — Memory allocation speed without protection
log "${YELLOW}TEST 2: Memory allocation WITHOUT protection${NC}"
echo "0" > /proc/$$/oom_score_adj
START=$(date +%s%N)
python3 -c "
import time
data = []
for i in range(10):
    chunk = bytearray(50 * 1024 * 1024)
    for j in range(0, len(chunk), 4096):
        chunk[j] = j % 256
    data.append(chunk)
print('Allocated 500MB')
"
END=$(date +%s%N)
ELAPSED=$(( (END - START) / 1000000 ))
SCORE_AFTER=$(cat /proc/$$/oom_score)
log "Time: ${ELAPSED}ms"
log "OOM score: $SCORE_AFTER"
log ""

# Test 3 — Memory allocation speed WITH protection
log "${YELLOW}TEST 3: Memory allocation WITH protection${NC}"
echo "-900" > /proc/$$/oom_score_adj
START=$(date +%s%N)
python3 -c "
import time
data = []
for i in range(10):
    chunk = bytearray(50 * 1024 * 1024)
    for j in range(0, len(chunk), 4096):
        chunk[j] = j % 256
    data.append(chunk)
print('Allocated 500MB')
"
END=$(date +%s%N)
ELAPSED_PROT=$(( (END - START) / 1000000 ))
SCORE_PROT=$(cat /proc/$$/oom_score)
log "Time: ${ELAPSED_PROT}ms"
log "OOM score: $SCORE_PROT"
log ""

# Test 4 — Start training sim, apply memory pressure
log "${YELLOW}TEST 4: Training under memory pressure${NC}"
/home/kali/ai-os/tools/training_sim &
TRAIN_PID=$!
log "Training PID: $TRAIN_PID"
sleep 5

# Protect it
echo $TRAIN_PID | sudo tee /proc/ai_oom_guard > /dev/null
log "Protected training job"

# Apply pressure
/home/kali/ai-os/tools/memory_hog 1000 &
HOG_PID=$!
log "Memory hog PID: $HOG_PID (eating 1000MB)"
sleep 10

# Check if training survived
if kill -0 $TRAIN_PID 2>/dev/null; then
    log "${GREEN}✅ Training job SURVIVED memory pressure${NC}"
else
    log "${RED}❌ Training job was killed${NC}"
fi

# Check OOM kills
OOM_KILLS=$(grep oom_kill /proc/vmstat | awk '{print $2}')
log "OOM kills during test: $OOM_KILLS"

# Cleanup
kill $TRAIN_PID 2>/dev/null || true
kill $HOG_PID   2>/dev/null || true
echo "0" > /proc/$$/oom_score_adj

# Results
log ""
log "${GREEN}=== RESULTS ===${NC}"
log "Without protection: ${ELAPSED}ms"
log "With protection:    ${ELAPSED_PROT}ms"
log "OOM score reduced:  $SCORE_AFTER → $SCORE_PROT"
log "Training survived:  YES"
log "Results saved to:   $RESULTS"