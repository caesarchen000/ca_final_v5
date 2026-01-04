#!/bin/bash
# Build script for gem5 with proper environment setup

set -e

# Set compilers to system versions
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++

# Set GEM5_HOME
export GEM5_HOME=..

# Ensure scons is in PATH
export PATH="/home/caesar/miniforge3/bin:$PATH"

# Use system Python for gem5 build (Python 3.8)
export PYTHON=/usr/bin/python3

# Set library paths - prioritize system libraries
export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"

# Change to gem5 root
cd "$GEM5_HOME"

# Run scons with full path
/home/caesar/miniforge3/bin/scons -j$(nproc) build/RISCV/gem5.fast

echo "Build completed successfully!"

