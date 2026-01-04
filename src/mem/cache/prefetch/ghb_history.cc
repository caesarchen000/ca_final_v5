/*
 * Implementation of the lightweight GHB history helper.
 */

#include "mem/cache/prefetch/ghb_history.hh"

#include <algorithm>

#include "base/logging.hh"

namespace gem5
{

namespace prefetch
{

GHBHistory::GHBHistory(unsigned history_size, unsigned pattern_length,
                       unsigned degree_, bool use_pc, unsigned page_bytes,
                       unsigned confidence_threshold)
    : historySize(std::max(1u, history_size)),
      patternLength(std::max(1u, pattern_length)),
      degree(std::max(1u, degree_)),
      usePC(use_pc),
      pageBytes(std::max(1u, page_bytes)),
      confidenceThreshold(std::min(100u, confidence_threshold)),
      history(historySize),
      head(0),
      filled(false),
      sequenceCounter(1)
{
}

void
GHBHistory::reset()
{
    for (auto &entry : history) {
        entry.addr = 0;
        entry.seq = 0;
        for (auto &link : entry.links) {
            link = LinkInfo{};
        }
    }
    for (auto &map : lastIndex) {
        map.clear();
    }
    head = 0;
    filled = false;
    sequenceCounter = 1;
    patternTable.clear();
}

void
GHBHistory::evictIndex(int32_t slot)
{
    removeIndexMappings(slot);
}

void
GHBHistory::removeIndexMappings(int32_t slot)
{
    GHBEntry &victim = history[slot];
    for (size_t i = 0; i < NumCorrelationKeys; ++i) {
        if (!victim.links[i].keyValid) {
            continue;
        }
        auto &indexMap = lastIndex[i];
        auto it = indexMap.find(victim.links[i].keyValue);
        if (it != indexMap.end() && it->second == slot) {
            indexMap.erase(it);
        }
        victim.links[i].keyValid = false;
    }
}

void
GHBHistory::assignCorrelation(GHBEntry &entry, int32_t slot,
                              CorrelationKey key, uint64_t value)
{
    const size_t idx = static_cast<size_t>(key);
    auto &link = entry.links[idx];
    link.prev = -1;
    link.prevSeq = 0;
    link.keyValid = true;
    link.keyValue = value;

    auto &indexMap = lastIndex[idx];
    auto it = indexMap.find(value);
    if (it != indexMap.end()) {
        link.prev = it->second;
        link.prevSeq = history[it->second].seq;
    }
    indexMap[value] = slot;
}

int32_t
GHBHistory::insert(const AccessInfo &access)
{
    if (historySize == 0) {
        return -1;
    }

    if (filled) {
        evictIndex(head);
    }

    const int32_t slot = head;
    GHBEntry &entry = history[slot];
    entry.addr = access.addr;
    entry.seq = sequenceCounter++;

    if (usePC && access.pc.has_value()) {
        assignCorrelation(entry, slot, CorrelationKey::PC,
                          access.pc.value());
    } else {
        entry.links[static_cast<size_t>(CorrelationKey::PC)] = LinkInfo{};
    }

    assignCorrelation(entry, slot, CorrelationKey::Page,
                      computePage(access.addr));

    head = (head + 1) % historySize;
    if (head == 0) {
        filled = true;
    }
    return slot;
}

bool
GHBHistory::buildPattern(int32_t index, CorrelationKey key,
                         std::vector<int64_t> &deltas) const
{
    deltas.clear();
    const size_t linkIdx = static_cast<size_t>(key);
    if (index < 0 || static_cast<size_t>(index) >= history.size()) {
        return false;
    }

    int32_t current = index;
    while (deltas.size() < patternLength) {
        const GHBEntry &entry = history[current];
        const LinkInfo &link = entry.links[linkIdx];
        if (link.prev < 0) {
            break;
        }
        const GHBEntry &prev_entry = history[link.prev];
        if (prev_entry.seq != link.prevSeq) {
            break;
        }

        deltas.push_back(static_cast<int64_t>(entry.addr) -
                         static_cast<int64_t>(prev_entry.addr));
        current = link.prev;
    }
    return !deltas.empty();
}

void
GHBHistory::updatePatternTable(const std::vector<int64_t> &chronological)
{
    if (chronological.size() < 3) {
        return;
    }

    // Update pattern table with sliding window of delta pairs
    // This captures more patterns for better prediction
    for (size_t i = 0; i + 2 < chronological.size(); ++i) {
        DeltaPair key{chronological[i], chronological[i + 1]};
        auto &entry = patternTable[key];
        entry.counts[chronological[i + 2]]++;
        entry.total++;
    }
    
    // Also update with overlapping windows to capture shorter patterns
    // This helps when patterns have varying lengths
    // Use multiple overlapping windows for better pattern coverage
    // Be more aggressive: update with all possible overlapping windows
    for (size_t offset = 1; offset < chronological.size() - 2 && offset < 4; ++offset) {
        for (size_t i = offset; i + 2 < chronological.size(); ++i) {
            DeltaPair key{chronological[i], chronological[i + 1]};
            auto &entry = patternTable[key];
            entry.counts[chronological[i + 2]]++;
            entry.total++;
        }
    }
    
    // Also capture patterns with different starting points for better coverage
    // This helps capture patterns that start at different positions
    if (chronological.size() >= 5) {
        // Use every other position for additional pattern coverage
        for (size_t i = 0; i + 2 < chronological.size(); i += 2) {
            DeltaPair key{chronological[i], chronological[i + 1]};
            auto &entry = patternTable[key];
            entry.counts[chronological[i + 2]]++;
            entry.total++;
        }
    }
}

bool
GHBHistory::findPatternMatch(const std::vector<int64_t> &chronological,
                             std::vector<int64_t> &predicted, unsigned max_predictions) const
{
    predicted.clear();
    if (chronological.size() < 2) {
        return false;
    }

    // Use all consecutive pairs from the chronological sequence
    // For each pair, look up in patternTable and collect predictions
    unsigned num_to_return = (max_predictions > 0) ? max_predictions : degree;
    
    // Aggregate predictions from all pairs with their confidence scores
    std::unordered_map<int64_t, std::pair<uint32_t, uint32_t>> aggregated_predictions;  // delta -> (count, total)
    
    unsigned max_confidence = 0;
    bool found_any_pattern = false;
    
    // Iterate through all consecutive pairs, weighting more recent pairs more heavily
    // Use all pairs but weight recent ones more heavily
    size_t start_idx = 0;  // Use all pairs for maximum coverage
    for (size_t i = start_idx; i + 1 < chronological.size(); ++i) {
        DeltaPair key{chronological[i], chronological[i + 1]};
        auto it = patternTable.find(key);
        if (it == patternTable.end()) {
            continue;
        }

        const PatternEntry &entry = it->second;
        if (entry.total == 0) {
            continue;
        }
        
        found_any_pattern = true;
        
        // Weight more recent pairs more heavily (exponential weighting)
        // Pairs closer to the end are more relevant
        size_t distance_from_end = chronological.size() - 1 - i;
        // More aggressive weighting: recent pairs get 2-6x weight, but all pairs contribute
        // Exponential weighting: very recent pairs get much higher weight
        unsigned weight = 1;
        if (distance_from_end < 3) {
            weight = 8 - distance_from_end;  // Most recent 3 pairs get 5-8x weight
        } else if (distance_from_end < 6) {
            weight = 5 - (distance_from_end - 3);  // Next 3 pairs get 2-4x weight
        } else if (distance_from_end < 10) {
            weight = 2;  // Pairs 6-10 positions back get 2x weight
        }
        // Pairs beyond 10 positions get 1x weight (still contribute)
        
        // Aggregate all candidates from this pair with weighting
        for (const auto &candidate : entry.counts) {
            int64_t delta = candidate.first;
            uint32_t count = candidate.second * weight;  // Apply weight to count
            uint32_t total = entry.total * weight;       // Apply weight to total
            
            if (aggregated_predictions.find(delta) == aggregated_predictions.end()) {
                aggregated_predictions[delta] = {0, 0};
            }
            aggregated_predictions[delta].first += count;
            aggregated_predictions[delta].second += total;
        }
    }
    
    if (!found_any_pattern) {
        return false;
    }
    
    // Convert aggregated predictions to candidates and sort by confidence
    std::vector<std::pair<int64_t, std::pair<uint32_t, uint32_t>>> candidates(
        aggregated_predictions.begin(), aggregated_predictions.end());
    
    // Sort by confidence (count/total ratio)
    std::sort(candidates.begin(), candidates.end(),
              [](const auto &a, const auto &b) {
                  double conf_a = (a.second.first * 100.0) / a.second.second;
                  double conf_b = (b.second.first * 100.0) / b.second.second;
                  return conf_a > conf_b;
              });
    
    // Collect high-confidence predictions (strict threshold)
    for (const auto &candidate : candidates) {
        unsigned confidence = (candidate.second.first * 100) / candidate.second.second;
        if (confidence > max_confidence) {
            max_confidence = confidence;
        }
        if (confidence >= confidenceThreshold) {
            predicted.push_back(candidate.first);
            if (predicted.size() >= num_to_return) {
                break;
            }
        }
    }

    // If no high-confidence predictions but we have candidates, use top one if it's reasonable
    // This helps with early learning phase and improves coverage
    if (predicted.empty() && !candidates.empty()) {
        unsigned top_confidence = (candidates[0].second.first * 100) / candidates[0].second.second;
        // Use top candidate if it has at least 20% confidence (more aggressive for better coverage)
        // Also use if we have multiple candidates with similar confidence
        if (top_confidence >= 20) {
            predicted.push_back(candidates[0].first);
            // If top confidence is reasonable, also add second candidate if it's close
            if (candidates.size() > 1 && top_confidence >= 30) {
                unsigned second_confidence = (candidates[1].second.first * 100) / candidates[1].second.second;
                if (second_confidence >= 20 && second_confidence >= top_confidence - 10) {
                    predicted.push_back(candidates[1].first);
                }
            }
        }
    }
    
    // If we have very high confidence (>60%), add additional predictions if available
    // This helps with complex patterns that have multiple likely next steps
    if (max_confidence > 60 && predicted.size() < num_to_return && candidates.size() > predicted.size()) {
        for (size_t i = predicted.size(); i < candidates.size() && predicted.size() < num_to_return; ++i) {
            unsigned conf = (candidates[i].second.first * 100) / candidates[i].second.second;
            // For high-confidence patterns, also accept medium-confidence secondary predictions
            // Be more aggressive: lower threshold for secondary predictions
            unsigned secondary_threshold = max_confidence > 80 ? (confidenceThreshold - 15) : 
                                          (max_confidence > 70 ? (confidenceThreshold - 10) : confidenceThreshold);
            if (conf >= secondary_threshold) {
                // Check if already in predicted
                bool already_added = false;
                for (int64_t p : predicted) {
                    if (p == candidates[i].first) {
                        already_added = true;
                        break;
                    }
                }
                if (!already_added) {
                    predicted.push_back(candidates[i].first);
                }
            }
        }
    }

    return !predicted.empty();
}

void
GHBHistory::fallbackPattern(const std::vector<int64_t> &chronological,
                            std::vector<int64_t> &predicted) const
{
    predicted.clear();
    if (chronological.empty()) {
        return;
    }

    // Return the most recent non-zero delta (for test compatibility)
    // The prefetcher will chain this delta multiple times for better performance
    for (auto it = chronological.rbegin(); it != chronological.rend(); ++it) {
        int64_t delta = *it;
        if (delta != 0) {
            predicted.push_back(delta);
            break;  // Return only the most recent delta
        }
    }
}

} // namespace prefetch
} // namespace gem5
