import os

def get_memory_info():
    info = {'total': 0, 'used': 0, 'free': 0, 'percent': 0}
    try:
        with open("/proc/ai_memory") as f:
            for line in f:
                if "Total:"    in line:
                    info['total']   = int(line.split()[1])
                if "Used:"     in line:
                    info['used']    = int(line.split()[1])
                if "Free:"     in line:
                    info['free']    = int(line.split()[1])
                if "Pressure:" in line:
                    info['percent'] = int(
                        line.split()[1].replace('%',''))
    except FileNotFoundError:
        print("[AI-OS] WARNING: memory_monitor.ko not loaded")
        print("[AI-OS] Fix: sudo insmod memory_monitor.ko")
    except Exception as e:
        print(f"[AI-OS] Error reading memory info: {e}")
    return info

def get_gpu_info():
    info = {'has_gpu': False, 'name': 'None',
            'used': 0, 'total': 0, 'percent': 0}
    try:
        with open("/proc/ai_gpu") as f:
            for line in f:
                if "GPU:"       in line:
                    info['name']    = line.split(":",1)[1].strip()
                if "VRAM Used"  in line:
                    info['used']    = int(line.split()[2])
                    info['has_gpu'] = True
                if "VRAM Total" in line:
                    info['total']   = int(line.split()[2])
                if "Pressure"   in line:
                    info['percent'] = int(
                        line.split()[1].replace('%',''))
    except FileNotFoundError:
        print("[AI-OS] WARNING: gpu_watcher.ko not loaded")
        print("[AI-OS] Fix: sudo insmod gpu_watcher.ko")
    except Exception as e:
        print(f"[AI-OS] Error reading GPU info: {e}")
    return info

def watch(interval=1):
    import time
    import signal
    import os
    running = True
    def stop(s, f):
        nonlocal running
        running = False
    signal.signal(signal.SIGINT, stop)
    while running:
        mem = get_memory_info()
        gpu = get_gpu_info()
        os.system("clear")
        print("AI-OS Monitor")
        print("=============")
        print(f"RAM:  {mem['used']}MB / {mem['total']}MB "
              f"({mem['percent']}%)")
        if gpu['has_gpu']:
            print(f"VRAM: {gpu['used']}MB / "
                  f"{gpu['total']}MB ({gpu['percent']}%)")
        else:
            print(f"GPU:  {gpu['name']}")
        time.sleep(interval)