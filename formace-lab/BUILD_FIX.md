# Fixing gem5 Build Issues with Conda

## Problem
When building gem5 with conda environment active, you may encounter:
1. Compiler detection issues (conda compiler not recognized)
2. Python library issues (libpython3.12.so.1.0 not found)

## Solution

The build script (`main.py`) has been updated to handle these issues automatically. However, if you still encounter the Python library error, you need to install system Python development packages:

```bash
sudo apt-get update
sudo apt-get install -y python3-dev python3.8-dev
```

Then rebuild:

```bash
cd formace-lab
export GEM5_HOME=..
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
venv/bin/python main.py build --target gem5 -j $(nproc)
```

## Alternative: Use System Python

If you prefer to avoid conda Python entirely:

1. Deactivate conda: `conda deactivate`
2. Ensure system Python 3.8 is available: `/usr/bin/python3 --version`
3. Install python3-dev: `sudo apt-get install -y python3-dev python3.8-dev`
4. Build: `venv/bin/python main.py build --target gem5 -j $(nproc)`

The build script will automatically:
- Use system compilers (`/usr/bin/gcc`, `/usr/bin/g++`)
- Set PYTHON_CONFIG to the correct path
- Configure library paths appropriately


