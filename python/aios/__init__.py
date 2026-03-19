"""
aios — AI-OS Python Package
Kernel-level OOM protection for AI training jobs
"""

from .core import protect, checkpoint, status, auto_checkpoint
from .monitor import watch, get_memory_info, get_gpu_info