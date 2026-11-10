// SPDX-FileCopyrightText: Copyright 2026 Fritchii and Lichii contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

#include "common/jit_warm.h"

namespace Common::JitWarm {
namespace {
std::mutex g_mutex;
std::unordered_map<u64, u8> g_compiled_on; ///< key -> mask of the cores that compiled it
u8 g_cores_seen{};
std::array<std::vector<u64>, 8> g_queues;
} // Anonymous namespace

bool Enabled() {
    static const bool enabled = [] {
        const char* const value = std::getenv("EDEN_NX2_JIT_WARM");
        return value == nullptr || value[0] != '0';
    }();
    return enabled;
}

void NoteCompiled(u64 key, u32 core) {
    const u8 bit = static_cast<u8>(1U << (core & 7));
    std::scoped_lock lock{g_mutex};
    g_cores_seen |= bit;
    u8& mask = g_compiled_on[key];
    mask |= bit;
    // Warm compiles don't queue for others; single-stepping blocks (bit 57) are never worth it.
    if (t_warming || ((key >> 57) & 1) != 0) {
        return;
    }
    for (u32 other = 0; other < 8; ++other) {
        const u8 other_bit = static_cast<u8>(1U << other);
        if ((g_cores_seen & other_bit) != 0 && (mask & other_bit) == 0 &&
            g_queues[other].size() < 400'000) {
            g_queues[other].push_back(key);
        }
    }
}

std::size_t TakeKeys(u32 core, std::vector<u64>& out, std::size_t max) {
    out.clear();
    std::scoped_lock lock{g_mutex};
    auto& queue = g_queues[core & 7];
    const u8 bit = static_cast<u8>(1U << (core & 7));
    std::size_t taken = 0;
    while (taken < queue.size() && out.size() < max) {
        const u64 key = queue[taken++];
        if ((g_compiled_on[key] & bit) == 0) { // skip what this core compiled meanwhile
            out.push_back(key);
        }
    }
    queue.erase(queue.begin(), queue.begin() + static_cast<std::ptrdiff_t>(taken));
    return out.size();
}

} // namespace Common::JitWarm
