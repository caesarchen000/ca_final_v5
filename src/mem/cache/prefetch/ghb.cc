#include "mem/cache/prefetch/ghb.hh"

#include <algorithm>

#include "base/logging.hh"
#include "params/GHBPrefetcher.hh"

namespace gem5
{

namespace prefetch
{

GHBPrefetcher::GHBPrefetcher(const GHBPrefetcherParams &p)
    : Queued(p),
      historySize(std::max(1u, p.history_size)),
      patternLength(std::max(1u, p.pattern_length)),
      degree(std::max(1u, p.degree)),
      usePC(p.use_pc),
      confidenceThreshold(
          std::min(100u,
                   std::max(0u, static_cast<unsigned>(p.confidence_threshold)))),
      historyHelper(historySize, patternLength, degree, usePC,
                    static_cast<unsigned>(pageBytes),
                    confidenceThreshold)
{
}

void
GHBPrefetcher::calculatePrefetch(
    const PrefetchInfo &pfi, std::vector<AddrPriority> &addresses,
    const CacheAccessor &cache)
{
    (void)cache;
    if (historyHelper.empty()) {
        return;
    }

    Addr block_addr = blockAddress(pfi.getAddr());

    GHBHistory::AccessInfo access{block_addr};
    if (usePC && pfi.hasPC()) {
        access.pc = pfi.getPC();
    }

    int32_t idx = historyHelper.insert(access);
    if (idx < 0) {
        return;
    }

    std::vector<int64_t> deltas;
    deltas.reserve(patternLength);
    bool hasPattern =
        historyHelper.buildPattern(idx, GHBHistory::CorrelationKey::PC, deltas);
    if (!hasPattern) {
        hasPattern = historyHelper.buildPattern(
            idx, GHBHistory::CorrelationKey::Page, deltas);
    }
    if (!hasPattern) {
        return;
    }

    std::vector<int64_t> chronological(deltas.rbegin(), deltas.rend());
    historyHelper.updatePatternTable(chronological);

    std::vector<int64_t> predicted;
    if (!historyHelper.findPatternMatch(chronological, predicted)) {
        historyHelper.fallbackPattern(chronological, predicted);
    }
    if (predicted.empty()) {
        return;
    }

    Addr next_addr = block_addr;
    for (int64_t delta : predicted) {
        if (delta == 0) {
            continue;
        }

        next_addr = static_cast<Addr>(
            static_cast<int64_t>(next_addr) + delta);

        if (!samePage(block_addr, next_addr)) {
            continue;
        }

        addresses.emplace_back(next_addr, 0);
    }
}

} // namespace prefetch
} // namespace gem5
