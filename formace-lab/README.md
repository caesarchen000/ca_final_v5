---
pdf-engine: xelatex
mainfont: Noto Sans
monofont: Noto Sans Mono
colorlinks: true
linkcolor: blue
urlcolor: magenta
citecolor: teal
---

# Computer Architecture Fall 2025: Final Project

- **Deadline**: 23:59, 2026-01-05 (no late submissions allowed)
- **Submission**: via NTU COOL, more info in [Submission Requirements](#submission-requirements) section below.
- **Contact**: If you have any questions, please email the TAs with the subject "[CA2025 final]".

  - YD: [r14943151@ntu.edu.tw](mailto:r14943151@ntu.edu.tw) (main creator and lead TA of the final project)
  - Circle: [r14943176@ntu.edu.tw](mailto:r14943176@ntu.edu.tw)
  - Alex: [r14943141@ntu.edu.tw](mailto:r14943141@ntu.edu.tw)

---

## Archive Content

The provided archive `CA2025_final.tgz` includes a customized source directory of the gem5 simulator
and a folder `formace-lab/` for the assignments.
The folder `formace-lab/` contains a unified CLI whose commands can invoke the assigned case studies.

To execute a command of the CLI, please make sure:

- You are inside the `formace-lab/` directory.
- Environment variable `GEM5_HOME` points to a gem5 checkout (i.e., the source directory).

---

## Prerequisites

### 1. Install Required Dependencies

gem5 requires various system packages. Install them using:

```bash
# Essential build tools and libraries for gem5
sudo apt install -y build-essential git m4 scons zlib1g zlib1g-dev \
    libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev \
    python3-dev python3-pip libboost-all-dev pkg-config

# RISC-V toolchain (required for compiling RISC-V binaries)
sudo apt install -y gcc-riscv64-linux-gnu g++-riscv64-linux-gnu
```

### 2. Extract the Archive

```bash
tar -zxvf CA2025_final.tgz
```

Your directory structure should look like:

```
ca-final/           # modified gem5
└── formace-lab/    # the repository to invoke commands for case studies
```

### 3. Build gem5.fast

Point environment variable `GEM5_HOME` to the source directory of the customized gem5 checkout.

Run the following commands:

```bash
# Build gem5.fast
cd formace-lab
python3 -m venv venv
venv/bin/python -m pip install -r requirements.txt
venv/bin/python main.py build --target gem5 --jobs $(nproc)
```

This will build `gem5.fast` at `$GEM5_HOME/build/RISCV/gem5.fast`.

If you are inside `formace-lab/`, `GEM5_HOME` could be set to `..` with `export GEM5_HOME=..`.

Example: `export GEM5_HOME=../ && venv/bin/python main.py <command line option for main.py>`

---

## Assignments

There are three case studies in this final project,
which are enumerated below with the corresponding command to execute each case study.
We recommend you to browse through the case studies first and then consult [Submission Requirement](#submission-requirements) for what you need to submit.

### Case 1 – Cache Experiments

#### 1.1 L1 Cache Size Sweep (demonstration, not graded)

**This case displays the summary of a Performance Monitor Unit (PMU) - Use it to learn how to extract metrics from `stats.txt`**

Please run the following command to invoke this case study, with `<student_id>` replaced by your student ID.

```bash
export GEM5_HOME=/path/to/gem5 && venv/bin/python main.py suite example --out output/<student_id>/case1/1-1
```

This command will:

1. Run the `mm` benchmark (matrix multiplication) with different L1 cache sizes (8kB, 32kB, 64kB)
2. **Display a PMU metrics summary** showing IPC, L1D MPKI, L2 MPKI, and Branch MPKI
3. Generate `all_results.csv` under `/output/<student_id>/case1/1-1/all_results.csv` with the extracted metrics

**Key metrics to extract from `stats.txt`:**

- `IPC` = Instructions Per Cycle = `numInsts / (simTicks / 1000)`
- `L1D MPKI` = L1 Data Cache Misses Per Kilo Instructions = `(l1d.overallMisses / numInsts) * 1000`
- `L2 MPKI` = L2 Cache Misses Per Kilo Instructions = `(l2.overallMisses / numInsts) * 1000`
- `Branch MPKI` = Branch Mispredictions Per Kilo Instructions = `(branchPred.condIncorrect / numInsts) * 1000`

**Note**: Case 1.1 is provided for demonstration purposes and thus **NOT** graded. Use it as a reference for the CSV format.

#### 1.2 L2 Cache Size Sweep

Please run the following command to sweep L2 cache size:

```bash
export GEM5_HOME=/path/to/gem5 && venv/bin/python main.py suite cache-size --benchmarks mm --l1d-size 32kB --l2-sizes 128kB 256kB 512kB --out output/<student_id>/case1/1-2
```

Output files can be found under `output/<student_id>/case1/1-2/`.

#### 1.3 Replacement Policy

Please run the following command to evaluate different cache replacement policies:

```bash
export GEM5_HOME=/path/to/gem5 && venv/bin/python main.py suite cache-policy --benchmarks cache_thrash --cache-policy LRU FIFO TreePLRU --l1d-size 32kB --l2-size 256kB --out output/<student_id>/case1/1-3
```

Output files can be found under `output/<student_id>/case1/1-3/`.

---

### Case 2 – Branch Prediction

The following three sub-cases compare LocalBP and BiModeBP branch predictors on different benchmarks.

For the interested, some technical details for the two branch predictors:

- LocalBP (local branch predictor): Predicts each branch based on its own past behavior by maintaining a per-branch history register, capturing patterns specific to individual branches (e.g., loop iteration counts).
- BiModeBP (bi-mode branch predictor): Reduces interference in global predictors by separating branches into "taken-biased" and "not-taken-biased" tables, using a choice predictor to select between them.

#### 2.1 Towers

Please run the following command to compare LocalBP and BiModeBP branch predictors on the `towers` benchmark:

```bash
export GEM5_HOME=/path/to/gem5 && venv/bin/python main.py suite branch --benchmarks towers --predictors LocalBP BiModeBP --out output/<student_id>/case2/2-1
```

#### 2.2 Correlated Branches

Please run the following command to compare LocalBP and BiModeBP branch predictors on the `branch_corr` benchmark:

```bash
export GEM5_HOME=/path/to/gem5 && venv/bin/python main.py suite branch --benchmarks branch_corr --predictors LocalBP BiModeBP --out output/<student_id>/case2/2-2
```

#### 2.3 Biased Branches

Please run the following command to compare LocalBP and BiModeBP branch predictors on the `branch_bias` benchmark:

```bash
export GEM5_HOME=/path/to/gem5 && venv/bin/python main.py suite branch --benchmarks branch_bias --predictors LocalBP BiModeBP --out output/<student_id>/case2/2-3
```

All statistics (IPC, branch MPKI, etc.) can be found in `all_results.csv` in the sub-cases under `output/<student_id>/case2/`.
Please fill in the respective `all_results.csv` files by extracting appropriate numbers from the corresponding `stats.txt` files.

---

### Case 3 – Prefetching

This case study first compares several prefetchers available in the gem5 simulator (sub-case 3.1),
and then asks you to implement a prefetcher based on the paper:

- [Data Cache Prefetching Using a Global History Buffer (Nesbit and Smith, HPCA 2004)](https://doi.org/10.1109/HPCA.2004.10030)

#### 3.1 gem5 Prefetchers Sweep

Please run the following command to compare different prefetchers in gemt on the `stream` benchmark:

```bash
export GEM5_HOME=/path/to/gem5 && venv/bin/python main.py suite prefetch-upstream --benchmarks stream --prefetchers StridePrefetcher AMPMPPrefetcher BOPPrefetcher --stride-degree 1 2 4 --out output/<student_id>/case3/3-1
```

Please fill in the respective `all_results.csv` files by extracting appropriate numbers from the corresponding `stats.txt` files.

#### 3.2 Implement the GHB Prefetcher

**Background**: This case study asks you to implement a simplified version of the Global History Buffer (GHB) prefetcher.

A basic, unoptimized implementation of the prefetcher is provided.
Your task is to improve the prefetcher by correctly implementing the technique in the above paper.

Please follow the steps below to set up the development environment:

1. **Build the GHB unit test binary**:

   ```bash
   export GEM5_HOME=/path/to/gem5 && venv/bin/python main.py build --target ghb-test --jobs $(nproc)
   ```

2. **Run the host-side unit tests**:

   ```bash
   venv/bin/python -m pytest tests/test_ghb.py -v
   ```

   - PASS ⇒ `4 passed` (BuildPatternFromPC, PageCorrelationWorksWithoutPC, PatternTablePredictsMostLikelyDelta, FallbackUsesRecentDeltas)
   - FAIL ⇒ pytest shows which specific test failed. Fix the bug before continuing.

3. **Performance comparison** (Case 3.2 - compare GHB vs Baseline stride prefetcher):

   ```bash
   export GEM5_HOME=/path/to/gem5 && venv/bin/python main.py suite prefetch-ghb --out output/<student_id>/case3/3-2
   ```

   This runs a fixed configuration comparing your GHB_LiteStudent against BaselineStride (degree=1) across 7 benchmarks:

   - **Benchmarks**: `stream`, `vvadd`, `qsort`, `towers`, `mm`, `binary_search`, `pointer_chase`
   - **Cache config**: L1I=32kB, L1D=32kB, L2=256kB

   The output displays a performance comparison table showing:

   - ΔIPC: IPC improvement (positive = better)
   - ΔL1D_MPKI: L1D MPKI change (negative = fewer misses, better)
   - ΔL2_MPKI: L2 MPKI change (negative = fewer misses, better)

   `all_results.csv` records detailed metrics for each benchmark+prefetcher combination.

---

## Practical Tips

- **Outputs**

  Every suite command writes:

  - `all_results.csv`
  - `benchmark/config/(nocache|caches)/stats.txt`

  **Note**: Suite commands (Case 1.2+) do NOT display PMU metric summaries. You must extract metrics from the generated `stats.txt` files yourself (see Case 1.1 for guidance on which stats to extract).

- **Building**

  Before running any experiments, build gem5 using:

  ```bash
  export GEM5_HOME=/path/to/gem5 && venv/bin/python main.py build --target <target>
  ```

  Available build targets:

  - `gem5` — build `gem5.fast`
  - `ghb-test` — build only the GHB unit test binary
  - `all` — build all binaries

- **Testing**

  The project includes four test suites in `tests/`:

  1.  **test_build.py** – Tests that gem5.fast and ghb_history.test.opt can be built
  2.  **test_ghb.py** – GHB prefetcher unit tests (4 tests for GHB logic)
  3.  **test_smoke.py** – End-to-end smoke tests for all 6 Cases (1.1, 1.2, 2.1, 2.2, 3.1, 3.2)
  4.  **test_main.py** – Unit tests for main.py structure and constants

  You can run tests with:

  ```bash
  GEM5_HOME=/path/to/gem5 venv/bin/python -m pytest tests/
  ```

---

## Submission Requirements

### What to Submit

You must submit **THREE** items in your submission zipped archive,
and you can use the `submit` command to automatically package all items (see [below](#creating-your-submission-archive)):

1. **summary.csv** - A CSV file summarizing ALL your experiment results.
2. **gem5 source code** - Your complete gem5 source implementation, including all files required to rebuild gem5.
3. **experiment outputs** - Complete `output/<your_student_id>/` directory with all `stats.txt` files from each experiment.

### Creating summary.csv

You must manually create `summary.csv` by extracting PMU metrics from each experiment's `stats.txt` file.
Please refer to Case 1.1 for guidance on which statistics to extract.
A template `summary.csv.example` is provided in `final/formace-lab/`.

**Key metrics to extract from stats.txt** (see Case 1.1 for formulas):

- `IPC` = Instructions Per Cycle = `system.cpu.numCycles / system.cpu.committedInsts`
- `L1D_MPKI` = L1 Data Cache Misses Per Kilo Instructions = `(system.cpu.dcache.overallMisses / system.cpu.committedInsts) * 1000`
- `L2_MPKI` = L2 Cache Misses Per Kilo Instructions = `(system.l2.overallMisses / system.cpu.committedInsts) * 1000`
- `Branch_MPKI` = Branch Mispredictions Per Kilo Instructions = `(system.cpu.branchPred.condIncorrect / system.cpu.committedInsts) * 1000`

### Directory Structure

Your `output/` directory **must** follow this structure:

```
output/
└── <your_student_id>/         # e.g., B12345678/
    ├── summary.csv            # *** REQUIRED: Your manually created summary ***
    ├── case1/
    │   ├── 1-1/               # Case 1.1 (Example)
    │   │   ├── mm/
    │   │   │   └── l1d_*/caches/stats.txt
    │   ├── 1-2/               # Case 1.2 (L2 Sweep)
    │   │   └── mm/
    │   │       └── l2_*/caches/stats.txt
    │   └── 1-3/               # Case 1.3 (Policy)
    │       └── cache_thrash/
    │           └── policy_*/caches/stats.txt
    ├── case2/
    │   ├── 2-1/               # Case 2.1
    │   ├── 2-2/               # Case 2.2
    │   └── 2-3/               # Case 2.3
    └── case3/
        ├── 3-1/               # Case 3.1
        └── 3-2/               # Case 3.2 (GHB)
```

### Creating Your Submission Archive

Use the built-in `submit` command to package your submission:

```bash
# Package your submission (requires GEM5_HOME to be set)
export GEM5_HOME=/path/to/gem5
venv/bin/python main.py submit <your_student_id>

# This creates: <your_student_id>_submission.tar.gz
# The archive includes:
#   1. Complete gem5 source code (with your implementations)
#   2. formace-lab/ directory
#   3. output/<your_student_id>/ with all experiment results
```

**IMPORTANT - Your Responsibility**:

The `submit` command is provided as a convenience tool,
but **YOU are responsible** for verifying your submission is correct:

1. **Verify the archive extracts correctly**:

   ```bash
   # Extract to a test directory
   mkdir /tmp/verify-submission
   cd /tmp/verify-submission
   tar -xzf <your_student_id>_submission.tar.gz
   ```

2. **Verify your code compiles**:

   ```bash
   cd /tmp/verify-submission
   export GEM5_HOME=$(pwd)
   cd formace-lab
   python3 -m venv venv
   venv/bin/pip install -r requirements.txt
   venv/bin/python main.py build --target gem5 --jobs $(nproc)
   ```

   - If build fails, your submission will receive **zero points**

3. **Verify all experiment outputs exist**:

   ```bash
   # Check that output/<your_student_id>/ has all required cases
   ls -R formace-lab/output/<your_student_id>/

   # Verify summary.csv exists
   ls formace-lab/output/<your_student_id>/summary.csv

   # Verify all stats.txt files exist
   find formace-lab/output/<your_student_id>/ -name "stats.txt" | wc -l
   ```

   - Missing `output/<your_student_id>/` → **zero points**
   - Missing `summary.csv` → **significant point deduction**
   - Missing stats.txt files → **partial credit only**

4. **Quick verification commands**:

   ```bash
   # Check archive contents
   tar -tzf <your_student_id>_submission.tar.gz | less

   # Check your output directory is included
   tar -tzf <your_student_id>_submission.tar.gz | grep "output/<your_student_id>" | head -20

   # Verify gem5 source is included
   tar -tzf <your_student_id>_submission.tar.gz | grep "^src/" | head -20
   ```

---

## Grading Criteria

### CSV Summaries — 30%

- Applies to: Cases 1.2, 1.3, 2.1, 2.2, 2.3, and 3.1
- You must fill out `summary.csv` and include the corresponding `stats.txt` files for each case.
- **5 points per case** (total 30 points)

### GHB Implementation — 70%

- Applies to: Case 3.2
- There will be 10 benchmarks (7 public + 3 hidden).
- You must modify the gem5 source code to implement the GHB prefetcher, build gem5, and pass all benchmarks.
- **7 points per benchmark**:
  - 3 points — correctness
  - 4 points — performance compared to TA's reference implementation
