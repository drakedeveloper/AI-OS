from setuptools import setup, find_packages

setup(
    name         = "aios",
    version      = "1.0.0",
    author       = "Trabelsi",
    description  = "Kernel-level OOM protection for AI training",
    packages     = find_packages(),
    python_requires = ">=3.8",
)