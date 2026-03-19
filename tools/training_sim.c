#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

volatile int running = 1;
void handle_signal(int sig) { (void)sig; running = 0; }

#define LAYER_SIZE_MB  50
#define NUM_LAYERS     8
#define CHECKPOINT_DIR "/tmp/ai_checkpoints"

typedef struct {
    char*  data;
    size_t size;
    int    layer_id;
} ModelLayer;

void touch_memory(char* mem, size_t size) {
    for (size_t i = 0; i < size; i += 4096)
        mem[i] = (char)(i % 256);
}

// Report checkpoint to kernel module
void report_checkpoint(int pid, int checkpoint_num) {
    FILE* f = fopen("/proc/ai_checkpoint", "w");
    if (!f) return;
    fprintf(f, "%d %d", pid, checkpoint_num);
    fclose(f);
}

// Save checkpoint to disk
void save_checkpoint(int epoch, ModelLayer* layers, int num_layers) {
    char path[256];
    snprintf(path, sizeof(path), "%s/checkpoint_epoch_%d.bin",
             CHECKPOINT_DIR, epoch);

    FILE* f = fopen(path, "wb");
    if (!f) return;

    // Write epoch number
    fwrite(&epoch, sizeof(int), 1, f);

    // Write first 1KB of each layer as "weights"
    for (int i = 0; i < num_layers; i++) {
        size_t save_size = 1024;
        fwrite(layers[i].data, 1, save_size, f);
    }
    fclose(f);
    printf("  >>> Checkpoint saved: %s\n", path);
}

int main(void) {
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);
    int pid = getpid();

    // Create checkpoint directory
    system("mkdir -p " CHECKPOINT_DIR);

    printf("=== AI TRAINING SIMULATOR ===\n");
    printf("PID: %d\n", pid);
    printf("Model: %d layers x %dMB = %dMB\n\n",
           NUM_LAYERS, LAYER_SIZE_MB,
           NUM_LAYERS * LAYER_SIZE_MB);

    // Load model layers
    ModelLayer layers[NUM_LAYERS];
    for (int i = 0; i < NUM_LAYERS; i++) {
        size_t size    = (size_t)LAYER_SIZE_MB * 1024 * 1024;
        layers[i].data = malloc(size);
        layers[i].size = size;
        layers[i].layer_id = i;

        if (!layers[i].data) {
            printf("FATAL: OOM at layer %d\n", i);
            return 1;
        }
        touch_memory(layers[i].data, size);
        printf("Layer %d loaded (%dMB)\n", i, LAYER_SIZE_MB);
    }

    printf("\nModel loaded. Starting training...\n");
    printf("Kernel module will protect this PID after "
           "%d seconds + %dMB RAM\n\n", 30, 200);

    int epoch      = 0;
    int checkpoint = 0;

    while (running) {
        epoch++;
        printf("Epoch %d training...\n", epoch);

        // Simulate forward + backward pass
        for (int i = 0; i < NUM_LAYERS && running; i++) {
            touch_memory(layers[i].data, layers[i].size);
            usleep(200000);
        }

        float loss = 1.0f / epoch;
        printf("Epoch %d done. Loss: %.4f\n", epoch, loss);

        // Checkpoint every 3 epochs
        if (epoch % 3 == 0) {
            checkpoint++;
            save_checkpoint(epoch, layers, NUM_LAYERS);
            report_checkpoint(pid, checkpoint);
            printf("  >>> Reported checkpoint %d to kernel\n\n",
                   checkpoint);
        }

        sleep(1);
    }

    // Cleanup
    printf("\nTraining stopped. Freeing memory...\n");
    for (int i = 0; i < NUM_LAYERS; i++)
        free(layers[i].data);

    printf("Done. Epochs: %d Checkpoints: %d\n",
           epoch, checkpoint);
    return 0;
}