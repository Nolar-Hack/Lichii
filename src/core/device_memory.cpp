// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>

#include "common/logging.h"
#include "common/memory_detect.h"
#include "common/thread.h"
#endif

#include "core/device_memory.h"
#include "hle/kernel/board/nintendo/nx/k_system_control.h"

namespace Core {

#ifdef HAS_NCE
constexpr size_t VirtualReserveSize = 1ULL << 38;
#else
constexpr size_t VirtualReserveSize = 1ULL << 39;
#endif

DeviceMemory::DeviceMemory()
    : buffer{Kernel::Board::Nintendo::Nx::KSystemControl::Init::GetIntendedMemorySize(),
             VirtualReserveSize} {
#ifdef _WIN32
    zero_trim_thread = std::jthread{[this](std::stop_token stop) { TrimZeroPages(stop); }};
#endif
}

DeviceMemory::~DeviceMemory() = default;

#ifdef _WIN32
// Windows can't punch holes in the guest memory section, so pages holding only zeros (games clear
// their heaps while booting: Ocarina leaves ~7 GB) stay resident until they leave the working set.
// A pass comes every 5 s while they keep turning up, backing off to once every 5 minutes.
void DeviceMemory::TrimZeroPages(std::stop_token stop) {
    using namespace std::chrono_literals;
    Common::SetCurrentThreadName("ZeroTrim");
    Common::SetCurrentThreadPriority(Common::ThreadPriority::Low);
    constexpr size_t Busy = size_t{64} << 20;
    std::mutex mutex;
    std::condition_variable_any wake;
    std::chrono::seconds interval = 5s;
    // EDEN_NX2_ZERO_TRIM_IDLE=0: the old timing. Otherwise (port of Fritchii v5.7) a pass waits
    // while the game maps memory in bulk: its working set query and ~20,000 VirtualUnlock calls
    // share the address space with the guest's map calls, which stalled 10-14 ms during a pass
    // (kill and load hitches). A pass that trims little backs off to 5 minutes, not 1: after boot
    // most passes trimmed 1-2 MiB for ~1 s of work.
    static const bool idle_timing = [] {
        const char* const value = std::getenv("EDEN_NX2_ZERO_TRIM_IDLE");
        return value == nullptr || value[0] != '0';
    }();
    const std::chrono::seconds max_interval = idle_timing ? 300s : 60s;
    while (true) {
        {
            std::unique_lock lock{mutex};
            wake.wait_for(lock, stop, interval, [] { return false; });
        }
        if (stop.stop_requested()) {
            return;
        }
        // Busy: over 32 MiB mapped, unmapped or protected in the last half second. At most 10
        // waits (5 s), then the pass runs anyway.
        for (int wait = 0; idle_timing && wait < 10; ++wait) {
            const u64 before = Common::HostMemory::MapActivityBytes();
            {
                std::unique_lock lock{mutex};
                wake.wait_for(lock, stop, 500ms, [] { return false; });
            }
            if (stop.stop_requested()) {
                return;
            }
            if (Common::HostMemory::MapActivityBytes() - before < (u64{32} << 20)) {
                break;
            }
        }
        const auto start = std::chrono::steady_clock::now();
        std::pair<size_t, size_t> result;
        try {
            result = buffer.TrimZeroPages();
        } catch (const std::bad_alloc&) {
            // A pass needs 32 MiB and more of scratch. When Windows runs out of commit, the
            // exception leaving this thread ended the emulator (#1346); the pass is only a saving.
            interval = std::chrono::seconds{60};
            const Common::ProcessMemoryUsage usage = Common::GetProcessMemoryUsage();
            LOG_WARNING(HW_Memory,
                        "Skipped trimming all-zero guest pages: out of memory. Windows commit is "
                        "{} of {} MiB (this process {} MiB); the limit is RAM plus the page file",
                        usage.system_commit >> 20, usage.system_commit_limit >> 20,
                        usage.commit >> 20);
            continue;
        }
        const auto [resident, trimmed] = result;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
        if (trimmed < Busy) {
            interval = (std::min)(interval * 2, max_interval);
            continue;
        }
        interval = 5s;
        LOG_INFO(HW_Memory,
                 "Trimmed {} MiB of all-zero guest pages out of {} MiB resident in {} ms; the kernel "
                 "zero-filled {} MiB so far",
                 trimmed >> 20, resident >> 20, ms, Common::HostMemory::ZeroFilledBytes() >> 20);
    }
}
#endif

} // namespace Core
