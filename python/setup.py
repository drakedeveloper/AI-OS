from setuptools import setup, find_packages

setup(
    name="aios-guard",
    version="1.0.1",
    author="Hamza Trabelsi",
    description="Kernel-level OOM protection for AI training",
    packages=find_packages(),
    python_requires=">=3.8",
)