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

    GHBHistory::AccessInfo access{block_addr, std::nullopt};
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
    bool hasMatch = historyHelper.findPatternMatch(chronological, predicted, degree);
    if (!hasMatch) {
        historyHelper.fallbackPattern(chronological, predicted);
    }
    if (predicted.empty()) {
        return;
    }

    // Generate prefetches: use predicted deltas and chain predictions
    Addr next_addr = block_addr;
    unsigned prefetches_generated = 0;
    
    // Use all predicted deltas first
    for (size_t i = 0; i < predicted.size() && prefetches_generated < degree; ++i) {
        int64_t delta = predicted[i];
        if (delta == 0) {
            continue;
        }

        next_addr = static_cast<Addr>(
            static_cast<int64_t>(next_addr) + delta);

        if (!samePage(block_addr, next_addr)) {
            continue;
        }

        addresses.emplace_back(next_addr, 0);
        prefetches_generated++;
    }
    
    // Chain prediction: use the primary delta (first predicted) to generate more prefetches
    // This is especially effective for stride-like patterns
    if (!predicted.empty() && prefetches_generated < degree) {
        int64_t primary_delta = predicted[0];
        if (primary_delta != 0) {
            Addr chain_addr = next_addr;
            // Chain up to degree prefetches using the primary delta
            while (prefetches_generated < degree) {
                chain_addr = static_cast<Addr>(
                    static_cast<int64_t>(chain_addr) + primary_delta);
                
                if (!samePage(block_addr, chain_addr)) {
                    break;
                }
                
                addresses.emplace_back(chain_addr, 0);
                prefetches_generated++;
            }
        }
    }
}

} // namespace prefetch
} // namespace gem5
