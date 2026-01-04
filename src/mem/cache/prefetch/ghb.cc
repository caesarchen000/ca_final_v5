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
    // Request multiple predictions for better coverage - be more aggressive
    bool hasMatch = historyHelper.findPatternMatch(chronological, predicted, degree * 4);
    
    // If we have a pattern match, use it; otherwise use fallback
    if (!hasMatch) {
        historyHelper.fallbackPattern(chronological, predicted);
    }
    
    if (predicted.empty()) {
        return;
    }

    // Generate prefetches: use predicted deltas and chain predictions
    Addr next_addr = block_addr;
    unsigned prefetches_generated = 0;
    unsigned target_degree = degree;
    
    // If we have high confidence (multiple high-confidence predictions), be more aggressive
    // Also be more aggressive for regular patterns (single consistent delta)
    if (hasMatch) {
        if (predicted.size() >= 2) {
            target_degree = degree + 8;  // Increase effective degree for high-confidence patterns
        } else if (predicted.size() == 1 && chronological.size() >= 3) {
            // Check if pattern is very regular (same delta repeated)
            bool is_regular = true;
            int64_t first_delta = chronological[chronological.size() - 1];
            size_t check_count = std::min(static_cast<size_t>(6), chronological.size() - 1);
            for (size_t i = chronological.size() - 2; i >= chronological.size() - check_count - 1 && i < chronological.size(); --i) {
                if (chronological[i] != first_delta) {
                    is_regular = false;
                    break;
                }
                if (i == 0) break;
            }
            if (is_regular) {
                target_degree = degree + 10;  // Very aggressive for regular stride patterns
            } else {
                // Even for irregular patterns, be more aggressive if we have a match
                target_degree = degree + 4;
            }
        }
    } else {
        // For fallback, also be more aggressive if pattern looks consistent
        if (chronological.size() >= 2 && chronological.back() != 0) {
            // Check if recent deltas are somewhat consistent
            if (chronological.size() >= 3) {
                int64_t last = chronological.back();
                int64_t prev = chronological[chronological.size() - 2];
                if (last == prev && last != 0) {
                    target_degree = degree + 4;  // More aggressive for consistent fallback
                } else {
                    target_degree = degree + 3;
                }
            } else {
                target_degree = degree + 3;
            }
        }
    }
    
    // Use all predicted deltas first (up to target_degree)
    for (size_t i = 0; i < predicted.size() && prefetches_generated < target_degree; ++i) {
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
    
    // Chain prediction: use the primary delta to generate more prefetches
    // Chain aggressively for both pattern matches and fallback (if fallback looks good)
    if (!predicted.empty() && prefetches_generated < target_degree) {
        int64_t primary_delta = predicted[0];
        if (primary_delta != 0) {
            Addr chain_addr = next_addr;
            // Chain up to target_degree prefetches using the primary delta
            // This is especially effective for stride-like patterns
            // Be more aggressive: chain even for fallback if the delta is consistent
            unsigned max_chain = hasMatch ? target_degree * 5 : degree * 2;  // Chain more for high-confidence
            while (prefetches_generated < target_degree) {
                chain_addr = static_cast<Addr>(
                    static_cast<int64_t>(chain_addr) + primary_delta);
                
                if (!samePage(block_addr, chain_addr)) {
                    break;
                }
                
                addresses.emplace_back(chain_addr, 0);
                prefetches_generated++;
                
                // Limit chaining for fallback to avoid pollution
                if (!hasMatch && prefetches_generated >= max_chain) {
                    break;
                }
            }
        }
    }
    
    // If we have multiple predicted deltas and room, use secondary delta for additional prefetches
    if (predicted.size() > 1 && prefetches_generated < target_degree && hasMatch) {
        int64_t secondary_delta = predicted[1];
        if (secondary_delta != 0 && secondary_delta != predicted[0]) {
            Addr chain_addr = next_addr;
            unsigned chain_count = 0;
            // Be more aggressive with secondary delta chaining
            unsigned max_secondary_chain = target_degree;  // Use target_degree for more aggressive chaining
            while (prefetches_generated < target_degree && chain_count < max_secondary_chain) {
                chain_addr = static_cast<Addr>(
                    static_cast<int64_t>(chain_addr) + secondary_delta);
                
                if (!samePage(block_addr, chain_addr)) {
                    break;
                }
                
                addresses.emplace_back(chain_addr, 0);
                prefetches_generated++;
                chain_count++;
            }
        }
    }
    
    // If we still have room and have a third predicted delta, use it too
    if (predicted.size() > 2 && prefetches_generated < target_degree && hasMatch) {
        int64_t tertiary_delta = predicted[2];
        if (tertiary_delta != 0 && tertiary_delta != predicted[0] && tertiary_delta != predicted[1]) {
            Addr chain_addr = next_addr;
            unsigned chain_count = 0;
            while (prefetches_generated < target_degree && chain_count < (degree / 2)) {
                chain_addr = static_cast<Addr>(
                    static_cast<int64_t>(chain_addr) + tertiary_delta);
                
                if (!samePage(block_addr, chain_addr)) {
                    break;
                }
                
                addresses.emplace_back(chain_addr, 0);
                prefetches_generated++;
                chain_count++;
            }
        }
    }
    
    // If we still have room and only one prediction, try alternating between primary and a variation
    // This helps with irregular patterns that have some structure
    if (predicted.size() == 1 && prefetches_generated < target_degree && hasMatch && chronological.size() >= 4) {
        int64_t primary_delta = predicted[0];
        // Look for a secondary pattern in the history
        if (primary_delta != 0) {
            // Try using the second-to-last delta as an alternative
            int64_t alt_delta = chronological[chronological.size() - 2];
            if (alt_delta != 0 && alt_delta != primary_delta) {
                Addr chain_addr = next_addr;
                unsigned alt_count = 0;
                while (prefetches_generated < target_degree && alt_count < (degree / 2)) {
                    chain_addr = static_cast<Addr>(
                        static_cast<int64_t>(chain_addr) + alt_delta);
                    
                    if (!samePage(block_addr, chain_addr)) {
                        break;
                    }
                    
                    addresses.emplace_back(chain_addr, 0);
                    prefetches_generated++;
                    alt_count++;
                }
            }
        }
    }
}

} // namespace prefetch
} // namespace gem5
