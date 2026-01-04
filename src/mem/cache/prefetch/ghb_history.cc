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

    for (size_t i = 0; i + 2 < chronological.size(); ++i) {
        DeltaPair key{chronological[i], chronological[i + 1]};
        auto &entry = patternTable[key];
        entry.counts[chronological[i + 2]]++;
        entry.total++;
    }
}

bool
GHBHistory::findPatternMatch(const std::vector<int64_t> &chronological,
                             std::vector<int64_t> &predicted) const
{
    predicted.clear();
    if (chronological.size() < 2) {
        return false;
    }

    DeltaPair key{chronological[chronological.size() - 2],
                  chronological.back()};
    auto it = patternTable.find(key);
    if (it == patternTable.end()) {
        return false;
    }

    const PatternEntry &entry = it->second;
    if (entry.total == 0) {
        return false;
    }

    std::vector<std::pair<int64_t, uint32_t>> candidates(
        entry.counts.begin(), entry.counts.end());
    std::sort(candidates.begin(), candidates.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    if (!candidates.empty()) {
        predicted.push_back(candidates[0].first);
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

    int64_t delta = chronological.back();
    if (delta != 0) {
        predicted.push_back(delta);
    }
}

} // namespace prefetch
} // namespace gem5
