# GHB Prefetcher Implementation Guide

## Overview

This guide covers **Case 3.2: GHB-Lite Bring-up**, where you will implement a simplified version of the Global History Buffer (GHB) prefetcher in gem5. Your implementation will be compared against a baseline StridePrefetcher to evaluate performance improvements across multiple benchmarks.

---

## Background & Paper Reference

The GHB prefetcher is based on the following research paper:

- **Data Cache Prefetching Using a Global History Buffer** 

  *Kyle J. Nesbit and James E. Smith, HPCA 2004*

  DOI: [10.1109/HPCA.2004.10030](https://ieeexplore.ieee.org/document/1410068)

This assignment implements a **simplified version** of the original GHB algorithm. The core ideas remain the same:
- Track memory access history in a global buffer
- Build delta patterns from correlated accesses
- Predict future deltas based on pattern matching
- Use both PC-based and page-based correlation

---

## Implementation Files

### Core Implementation

You will need to implement the GHB algorithm in the following files:

- **`src/mem/cache/prefetch/ghb_history.hh`**
  Header file defining the `GHBHistory` class and data structures (history buffer, index table, pattern table, correlation keys)

- **`src/mem/cache/prefetch/ghb_history.cc`**
  Implementation of core GHB logic including:
  - `insert()` - Insert new memory access into history
  - `buildPattern()` - Extract delta pattern from history chain
  - `updatePatternTable()` - Update pattern statistics
  - `findPatternMatch()` - Predict next delta based on pattern table
  - `fallbackPattern()` - Fallback prediction when no pattern matches

- **`src/mem/cache/prefetch/ghb.hh`**
  Wrapper header for GHBPrefetcher class

- **`src/mem/cache/prefetch/ghb.cc`**
  Prefetcher wrapper that integrates GHBHistory with gem5's prefetch infrastructure

### Configuration

- **`src/mem/cache/prefetch/Prefetcher.py`** (lines 168-182)
  Python configuration defining GHBPrefetcher parameters:
  - `history_size = 256` - Number of entries in the global history buffer
  - `pattern_length = 16` - Number of historical deltas to track
  - `degree = 4` - Number of prefetches to generate per miss
  - `use_pc = True` - Enable PC-based correlation
  - `confidence_threshold = 60%` - Minimum confidence for pattern predictions

---

## Building Your Implementation

After modifying the GHB source files, rebuild gem5:

```bash
# From formace-lab/ directory
export GEM5_HOME=..
venv/bin/python main.py build --target gem5 -j $(nproc)
```

If you modify any `.cc` or `.hh` files and the build doesn't pick up your changes, you may need to force a clean rebuild:

```bash
# Remove the build cache (do this if changes aren't being picked up)
rm -rf $GEM5_HOME/build

# Then rebuild
export GEM5_HOME=..
venv/bin/python main.py build --target gem5 -j $(nproc)
```

---

## Running Case 3.2

Once built, run the GHB performance comparison:

```bash
# From formace-lab/ directory
export GEM5_HOME=..
venv/bin/python main.py suite prefetch-ghb --out output/<student_id>/case3/3-2
```

### Test Configuration

Case 3.2 uses a **fixed configuration** to ensure fair comparison:

- **Benchmarks**: `stream`, `vvadd`, `qsort`, `towers`, `mm`, `binary_search`, `pointer_chase`
- **Cache Configuration**: L1I=32kB, L1D=32kB, L2=256kB
- **Baseline**: StridePrefetcher with degree=1
- **Your Implementation**: GHBPrefetcher (labeled as "GHB_LiteStudent")

The test runs each benchmark twice (once with baseline, once with GHB) and compares performance.

---

## Understanding the Results

### Golden Output Validation

**Before performance metrics**, the test automatically validates correctness by comparing your GHB implementation's output against known-good baseline results:

```
================================================================================
Validating Correctness (Golden Output Comparison)
================================================================================
  ✓ stream         (BaselineStri)
  ✓ stream         (GHB_LiteStud)
  ✓ vvadd          (BaselineStri)
  ✓ vvadd          (GHB_LiteStud)
  ✓ qsort          (BaselineStri)
  ✓ qsort          (GHB_LiteStud)
  ✓ towers         (BaselineStri)
  ✓ towers         (GHB_LiteStud)
  ✓ mm             (BaselineStri)
  ✓ mm             (GHB_LiteStud)
  ✓ binary_search  (BaselineStri)
  ✓ binary_search  (GHB_LiteStud)
  ✓ pointer_chase  (BaselineStri)
  ✓ pointer_chase  (GHB_LiteStud)
================================================================================
✓ All golden outputs MATCH - Implementation is CORRECT
```

**What is being validated?**

Each benchmark prints its actual computation results (not just checksums) for verification:

- **mm**: Complete 64×64 result matrix
- **stream**: Sampled array values after all operations
- **vvadd**: Sampled vector addition results
- **qsort**: Sampled sorted array elements
- **towers**: Total move count (262143 for 18 disks)
- **binary_search**: First 100 search results
- **pointer_chase**: First 64 node traversal values

The test compares these outputs byte-by-byte against golden files in `golden/*.golden`. These golden files were generated using the baseline StridePrefetcher configuration.

**Important**: If you see ✗ marks or "Golden output mismatch", your GHB implementation has bugs that are **corrupting program results**. This must be fixed before considering performance metrics.

### Performance Table

After validation passes, you will see a performance comparison table:

```
================================================================================
GHB Performance Comparison (GHB vs Baseline Stride)
================================================================================
Benchmark      |  Base_IPC |  GHB_IPC |     ΔIPC |   ΔL1D_MPKI |   ΔL2_MPKI
--------------------------------------------------------------------------------
stream         |     1.068 |    1.233 |  +15.49% |      -0.15% |    -11.77%
vvadd          |     1.248 |    1.399 |  +12.16% |      -0.01% |     -9.32%
qsort          |     0.886 |    0.944 |   +6.54% |      -3.50% |    -20.80%
towers         |     3.028 |    3.134 |   +3.51% |      -1.81% |    -18.79%
mm             |     2.855 |    2.939 |   +2.93% |      -0.09% |    -12.77%
binary_search  |     0.954 |    0.973 |   +1.93% |      -2.09% |    -18.16%
pointer_chase  |     0.957 |    1.001 |   +4.67% |      -3.27% |    -17.22%
================================================================================
Average IPC Improvement: +6.75%
================================================================================
```

- **Base_IPC**: Instructions Per Cycle with baseline StridePrefetcher
- **GHB_IPC**: Instructions Per Cycle with your GHB implementation
- **ΔIPC**: Percentage improvement (positive = better performance)
  - Higher is better
  - Shows how much faster your prefetcher makes the program run

- **ΔL1D_MPKI**: Change in L1 Data cache Misses Per Kilo Instructions
  - Negative values = fewer misses (better)
  - Shows if your prefetcher reduces L1D cache misses

- **ΔL2_MPKI**: Change in L2 cache Misses Per Kilo Instructions
  - Negative values = fewer misses (better)
  - More important metric - reducing L2 misses directly improves performance

### Output Files

The suite command generates:

- **`output/<student_id>/case3/3-2/all_results.csv`**
  Complete metrics for all benchmark runs (IPC, MPKI, prefetch accuracy, etc.)

- **`output/<student_id>/case3/3-2/<benchmark>/<config>/caches/stats.txt`**
  Detailed gem5 statistics for each individual run

---

## Performance Expectations

The example output above shows a reference implementation achieving **+6.75% average IPC improvement**. 
Your GHB implementation should show **measurable improvement** over the baseline StridePrefetcher:

- **Average IPC improvement**: Aim for **+5% or higher**
- Different benchmarks will show different improvements:
  - **Stream-like workloads** (`stream`, `vvadd`): Expect strong improvements (10-15%+) since GHB can learn regular stride patterns just like StridePrefetcher.
  - **Irregular patterns** (`qsort`, `towers`, `mm`): Moderate improvements (3-7%) because GHB's pattern matching can capture more complex access sequences.
  - **Random access** (`binary_search`, `pointer_chase`): Smaller improvements (1-5%). Neither prefetcher helps much, but GHB may predict some local patterns.

---

## Tips for Implementation

### Where to Start

1. Read the paper, focusing on Sections 1–3, which describe the core algorithm.
2. Review the existing code structure in `ghb_history.hh`.
3. Examine the method signatures to understand the expected behavior.
4. Implement the methods incrementally and verify correctness at each step.

### Key Implementation Points

- History buffer management: maintain the circular buffer and correlation chains correctly.
- Delta calculation: compute deltas as signed differences between block addresses.
- Pattern table updates: track mappings from delta pairs to the next delta along with their occurrence counts.
- Confidence-based prediction: issue prefetches only when the pattern confidence exceeds a threshold.
- Fallback strategy: provide a simple fallback when no pattern match is found in the pattern table.

### Common Pitfalls

- Off-by-one errors in circular buffer indexing.
- Stale pointers when history entries are evicted and reused.
- Integer overflow during delta computation (use signed 64-bit integers).
- Page boundary violations caused by prefetching across page boundaries.
- Accessing empty patterns without proper checks.

---

Good luck with your implementation!
