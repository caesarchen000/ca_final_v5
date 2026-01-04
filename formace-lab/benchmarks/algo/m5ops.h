// gem5 m5ops for RISC-V (pseudo-instruction interface)
// Use to mark ROI boundaries in benchmarks

#ifndef M5OPS_H
#define M5OPS_H

// m5_dump_reset_stats: Dump current stats and reset counters
// Call this BEFORE golden output to exclude printf overhead from stats
#define m5_dump_reset_stats() \
    asm volatile (".long 0x8400007b" ::: "memory")

// m5_reset_stats: Reset stats counters (without dump)
#define m5_reset_stats() \
    asm volatile (".long 0x8000007b" ::: "memory")

// m5_dump_stats: Dump stats (without reset)
#define m5_dump_stats() \
    asm volatile (".long 0x8200007b" ::: "memory")

#endif // M5OPS_H
