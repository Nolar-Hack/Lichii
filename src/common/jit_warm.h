// SPDX-FileCopyrightText: Copyright 2026 Fritchii and Lichii contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <vector>

#include "common/common_types.h"

/// JIT warm-up (port of Fritchii v5.7's EDEN_NX2_JIT_WARM). Each emulated core has its own JIT,
/// so code one core compiled is compiled again by every other core the first time it runs it
/// there: bursts of 60-180 ms frames after area loads in Ocarina of Time. Compiles on demand
/// queue their block key for the other cores, and an idle core compiles them ahead of time.
/// On by default; EDEN_NX2_JIT_WARM=0 turns it off.
namespace Common::JitWarm {

/// Set on a CPU core's thread while it warm-compiles (PhysicalCore::Idle).
inline thread_local bool t_warming{};

[[nodiscard]] bool Enabled();

/// A JIT on `core` compiled block `key` (an A64 LocationDescriptor value) on demand.
void NoteCompiled(u64 key, u32 core);

/// Up to `max` keys other cores compiled that `core` has not, oldest first. Returns how many.
std::size_t TakeKeys(u32 core, std::vector<u64>& out, std::size_t max);

} // namespace Common::JitWarm
