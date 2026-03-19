import os
import time

GUARD_PROC      = "/proc/ai_oom_guard"
CHECKPOINT_PROC = "/proc/ai_checkpoint"

def protect(pid=None):
    if pid is None:
        pid = os.getpid()
    try:
        with open(GUARD_PROC, "w") as f:
            f.write(str(pid))
        print(f"[AI-OS] PID {pid} protected via kernel module")
        return True
    except FileNotFoundError:
        print("[AI-OS] WARNING: ai_oom_guard.ko not loaded")
        print("[AI-OS] Falling back to user space protection")
    except Exception:
        pass
    try:
        with open(f"/proc/{pid}/oom_score_adj", "w") as f:
            f.write("-900")
        print(f"[AI-OS] PID {pid} protected via oom_score_adj")
        return True
    except PermissionError:
        print("[AI-OS] ERROR: Permission denied — run with sudo")
        return False
    except Exception as e:
        print(f"[AI-OS] ERROR: {e}")
        return False

def checkpoint(num=None):
    pid = os.getpid()
    try:
        with open(CHECKPOINT_PROC, "w") as f:
            f.write(f"{pid} {num or 0}")
    except FileNotFoundError:
        pass
    except Exception:
        pass

def status():
    time.sleep(0.5)
    pid = os.getpid()
    try:
        score = open(f"/proc/{pid}/oom_score").read().strip()
        adj   = open(f"/proc/{pid}/oom_score_adj").read().strip()
        prot  = "PROTECTED" if int(adj) <= -500 else "UNPROTECTED"
        print(f"[AI-OS] PID:{pid} OOM_SCORE:{score} "
              f"ADJ:{adj} STATUS:{prot}")
    except Exception as e:
        print(f"[AI-OS] Cannot read status: {e}")

def auto_checkpoint(model, optimizer, epoch,
                    path, interval=5):
    import torch
    if epoch % interval == 0:
        torch.save({
            'epoch':     epoch,
            'model':     model.state_dict(),
            'optimizer': optimizer.state_dict(),
        }, path)
        checkpoint(epoch // interval)
        print(f"[AI-OS] Checkpoint saved: {path}")