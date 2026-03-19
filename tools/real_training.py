import torch
import torch.nn as nn
import torch.optim as optim
import os
import time
import signal
import sys
import sys
sys.path.insert(0, '/home/kali/ai-os/python')
import aios

# Add after print(f"Device: ...")
aios.protect()
aios.status()
PID = os.getpid()
CHECKPOINT_DIR = "/tmp/ai_checkpoints"
os.makedirs(CHECKPOINT_DIR, exist_ok=True)

print(f"=== REAL PYTORCH TRAINING ===")
print(f"PID: {PID}")
print(f"PyTorch: {torch.__version__}")
print(f"Device: {'GPU' if torch.cuda.is_available() else 'CPU'}\n")

def report_checkpoint(pid, num):
    try:
        with open("/proc/ai_checkpoint", "w") as f:
            f.write(f"{pid} {num}")
        print(f"  >>> Checkpoint {num} reported to kernel")
    except:
        pass

def save_checkpoint(epoch, model, path):
    torch.save({
        'epoch': epoch,
        'model': model.state_dict(),
    }, path)

# Large model — uses significant RAM
class BigModel(nn.Module):
    def __init__(self):
        super().__init__()
        self.layers = nn.Sequential(
            nn.Linear(1024, 4096),
            nn.ReLU(),
            nn.Linear(4096, 4096),
            nn.ReLU(),
            nn.Linear(4096, 4096),
            nn.ReLU(),
            nn.Linear(4096, 2048),
            nn.ReLU(),
            nn.Linear(2048, 10)
        )
    def forward(self, x):
        return self.layers(x)

model     = BigModel()
optimizer = optim.Adam(model.parameters(), lr=0.001)
criterion = nn.CrossEntropyLoss()

# Generate fake dataset — large enough to use real RAM
print("Loading dataset into RAM...")
X = torch.randn(10000, 1024)
y = torch.randint(0, 10, (10000,))
print(f"Dataset: {X.shape} — "
      f"{X.element_size() * X.nelement() / 1024 / 1024:.1f} MB\n")

running    = True
checkpoint = 0

def handle_signal(sig, frame):
    global running
    running = False

signal.signal(signal.SIGINT, handle_signal)

epoch = 0
while running:
    epoch += 1
    start = time.time()

    # Mini-batch training
    total_loss = 0
    batch_size = 256
    for i in range(0, len(X), batch_size):
        xb = X[i:i+batch_size]
        yb = y[i:i+batch_size]

        optimizer.zero_grad()
        out  = model(xb)
        loss = criterion(out, yb)
        loss.backward()
        optimizer.step()
        total_loss += loss.item()

    elapsed = time.time() - start
    avg_loss = total_loss / (len(X) // batch_size)

    print(f"Epoch {epoch:3d} | "
          f"Loss: {avg_loss:.4f} | "
          f"Time: {elapsed:.2f}s")

    # Checkpoint every 5 epochs
    if epoch % 5 == 0:
        path = f"{CHECKPOINT_DIR}/pytorch_epoch_{epoch}.pt"
        aios.auto_checkpoint(model, optimizer, epoch, path)

    time.sleep(0.5)

print(f"\nTraining stopped. Epochs: {epoch}")